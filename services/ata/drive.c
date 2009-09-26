/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <esc/common.h>
#include <esc/debug.h>
#include <esc/fileio.h>
#include <esc/service.h>
#include <esc/proc.h>
#include <esc/ports.h>
#include "drive.h"
#include "ata.h"
#include "atapi.h"

#define ATA_WAIT_TIMEOUT	500	/* ms */

static bool drive_isBusResponding(sATADrive* drive);
static bool drive_identify(sATADrive *drive,u8 cmd);

void drive_detect(sATADrive *drives,u32 count) {
	u32 i,s;
	u16 buffer[256];
	s = 0;
	for(i = 0; i < count; i++) {
		/* check for each bus if it's present */
		if(i % 2 == 0) {
			ATA_PR1("Checking if bus %d is present",i / 2);
			if(!drive_isBusResponding(drives + i)) {
				ATA_PR1("Not present; skipping master & slave");
				i++;
				continue;
			}
		}

		ATA_PR1("Its present, sending 'IDENTIFY DEVICE' to device %d",i);
		/* first, identify the drive */
		if(!drive_identify(drives + i,COMMAND_IDENTIFY)) {
			ATA_PR1("Sending 'IDENTIFY PACKET DEVICE' to device %d",i);
			/* if that failed, try IDENTIFY PACKET DEVICE. Perhaps its an ATAPI-drive */
			if(!drive_identify(drives + i,COMMAND_IDENTIFY_PACKET)) {
				ATA_PR1("Device seems not to be present");
				continue;
			}
		}

		/* if it is present, read the partition-table */
		drives[i].present = 1;
		if(!drives[i].info.general.isATAPI) {
			drives[i].secSize = ATA_SEC_SIZE;
			drives[i].rwHandler = ata_readWrite;
			ATA_PR1("Device %d is an ATA-device; reading partition-table",i);
			if(!ata_readWrite(drives + i,false,buffer,0,1)) {
				drives[i].present = 0;
				ATA_PR1("Drive %d: Unable to read partition-table!",i);
				continue;
			}

			/* copy partitions to mem */
			ATA_PR2("Parsing partition-table");
			part_fillPartitions(drives[i].partTable,buffer);
		}
		else {
			drives[i].secSize = ATAPI_SEC_SIZE;
			drives[i].rwHandler = atapi_read;
			/* pretend that the cd has 1 partition */
			drives[i].partTable[0].present = 1;
			drives[i].partTable[0].start = 0;
			drives[i].partTable[0].size = atapi_getCapacity(drives + i);
			ATA_PR1("Device %d is an ATAPI-device; size=%d",i,drives[i].partTable[0].size);
		}
	}
}

static bool drive_isBusResponding(sATADrive* drive) {
	s32 i;
	for(i = 1; i >= 0; i--) {
		/* begin with slave. master should respond if there is no slave */
		outByte(drive->basePort + REG_DRIVE_SELECT,i << 4);
		ata_wait(drive);

		/* write some arbitrary values to some registers */
		outByte(drive->basePort + REG_ADDRESS1,0xF1);
		outByte(drive->basePort + REG_ADDRESS2,0xF2);
		outByte(drive->basePort + REG_ADDRESS3,0xF3);

		/* if we can read them back, the bus is present */
		if(inByte(drive->basePort + REG_ADDRESS1) == 0xF1 &&
			inByte(drive->basePort + REG_ADDRESS2) == 0xF2 &&
			inByte(drive->basePort + REG_ADDRESS3) == 0xF3)
			return true;
	}
	return false;
}

static bool drive_identify(sATADrive *drive,u8 cmd) {
	u8 status;
	u16 *data;
	u32 x;
	u16 basePort = drive->basePort;

	ATA_PR2("Selecting device with port 0x%x",drive->basePort);
	outByte(drive->basePort + REG_DRIVE_SELECT,drive->slaveBit << 4);
	ata_wait(drive);

	/* disable interrupts */
	outByte(basePort + REG_CONTROL,CTRL_INTRPTS_ENABLED);

	/* check wether the drive exists */
	outByte(basePort + REG_COMMAND,cmd);
	status = inByte(basePort + REG_STATUS);
	ATA_PR1("Got 0x%x from status-port",status);
	if(status == 0)
		return false;
	else {
		u32 time = 0;
		ATA_PR2("Waiting for ATA-device");
		/* wait while busy; the other bits aren't valid while busy is set */
		while((inByte(basePort + REG_STATUS) & CMD_ST_BUSY) && time < ATA_WAIT_TIMEOUT) {
			time += 20;
			sleep(20);
		}
		/* wait a bit */
		ata_wait(drive);
		/* wait until ready (or error) */
		while(((status = inByte(basePort + REG_STATUS)) & (CMD_ST_BUSY | CMD_ST_DRQ)) != CMD_ST_DRQ &&
				time < ATA_WAIT_TIMEOUT) {
			if(status & CMD_ST_ERROR) {
				ATA_PR1("Error-bit in status set");
				return false;
			}
			time += 20;
			sleep(20);
		}
		if((status & (CMD_ST_BUSY | CMD_ST_DRQ)) != CMD_ST_DRQ) {
			ATA_PR1("Timeout (%d ms)",ATA_WAIT_TIMEOUT);
			return false;
		}

		ATA_PR2("Reading information about device");
		/* drive ready */
		data = (u16*)&drive->info;
		for(x = 0; x < 256; x++)
			data[x] = inWord(basePort + REG_DATA);

		/* we don't support CHS atm */
		if(drive->info.capabilities.LBA == 0) {
			ATA_PR1("Device doesn't support LBA");
			return false;
		}

		return true;
	}
}

#if DEBUGGING

void drive_dbg_printInfo(sATADrive *drive) {
	u32 i;
	debugf("oldCurCylinderCount = %u\n",drive->info.oldCurCylinderCount);
	debugf("oldCurHeadCount = %u\n",drive->info.oldCurHeadCount);
	debugf("oldCurSecsPerTrack = %u\n",drive->info.oldCurSecsPerTrack);
	debugf("oldCylinderCount = %u\n",drive->info.oldCylinderCount);
	debugf("oldHeadCount = %u\n",drive->info.oldHeadCount);
	debugf("oldSecsPerTrack = %u\n",drive->info.oldSecsPerTrack);
	debugf("oldswDMAActive = %u\n",drive->info.oldswDMAActive);
	debugf("oldswDMASupported = %u\n",drive->info.oldswDMASupported);
	debugf("oldUnformBytesPerSec = %u\n",drive->info.oldUnformBytesPerSec);
	debugf("oldUnformBytesPerTrack = %u\n",drive->info.oldUnformBytesPerTrack);
	debugf("curmaxSecsPerIntrpt = %u\n",drive->info.curmaxSecsPerIntrpt);
	debugf("maxSecsPerIntrpt = %u\n",drive->info.maxSecsPerIntrpt);
	debugf("firmwareRev = '");
	for(i = 0; i < 8; i += 2)
		debugf("%c%c",drive->info.firmwareRev[i + 1],drive->info.firmwareRev[i]);
	debugf("'\n");
	debugf("modelNo = '");
	for(i = 0; i < 40; i += 2)
		debugf("%c%c",drive->info.modelNo[i + 1],drive->info.modelNo[i]);
	debugf("'\n");
	debugf("serialNumber = '");
	for(i = 0; i < 20; i += 2)
		debugf("%c%c",drive->info.serialNumber[i + 1],drive->info.serialNumber[i]);
	debugf("'\n");
	debugf("majorVer = 0x%02x\n",drive->info.majorVersion.raw);
	debugf("minorVer = 0x%02x\n",drive->info.minorVersion);
	debugf("general.isATAPI = %u\n",drive->info.general.isATAPI);
	debugf("general.remMediaDevice = %u\n",drive->info.general.remMediaDevice);
	debugf("mwDMAMode0Supp = %u\n",drive->info.mwDMAMode0Supp);
	debugf("mwDMAMode0Sel = %u\n",drive->info.mwDMAMode0Sel);
	debugf("mwDMAMode1Supp = %u\n",drive->info.mwDMAMode1Supp);
	debugf("mwDMAMode1Sel = %u\n",drive->info.mwDMAMode1Sel);
	debugf("mwDMAMode2Supp = %u\n",drive->info.mwDMAMode2Supp);
	debugf("mwDMAMode2Sel = %u\n",drive->info.mwDMAMode2Sel);
	debugf("minMwDMATransTimePerWord = %u\n",drive->info.minMwDMATransTimePerWord);
	debugf("recMwDMATransTime = %u\n",drive->info.recMwDMATransTime);
	debugf("minPIOTransTime = %u\n",drive->info.minPIOTransTime);
	debugf("minPIOTransTimeIncCtrlFlow = %u\n",drive->info.minPIOTransTimeIncCtrlFlow);
	debugf("multipleSecsValid = %u\n",drive->info.multipleSecsValid);
	debugf("word88Valid = %u\n",drive->info.word88Valid);
	debugf("words5458Valid = %u\n",drive->info.words5458Valid);
	debugf("words6470Valid = %u\n",drive->info.words6470Valid);
	debugf("userSectorCount = %u\n",drive->info.userSectorCount);
	debugf("Capabilities:\n");
	debugf("	DMA = %u\n",drive->info.capabilities.DMA);
	debugf("	LBA = %u\n",drive->info.capabilities.LBA);
	debugf("	IORDYDis = %u\n",drive->info.capabilities.IORDYDisabled);
	debugf("	IORDYSup = %u\n",drive->info.capabilities.IORDYSupported);
	debugf("Features:\n");
	debugf("	APM = %u\n",drive->info.features.apm);
	debugf("	autoAcousticMngmnt = %u\n",drive->info.features.autoAcousticMngmnt);
	debugf("	CFA = %u\n",drive->info.features.cfa);
	debugf("	devConfigOverlay = %u\n",drive->info.features.devConfigOverlay);
	debugf("	deviceReset = %u\n",drive->info.features.deviceReset);
	debugf("	downloadMicrocode = %u\n",drive->info.features.downloadMicrocode);
	debugf("	flushCache = %u\n",drive->info.features.flushCache);
	debugf("	flushCacheExt = %u\n",drive->info.features.flushCacheExt);
	debugf("	hostProtArea = %u\n",drive->info.features.hostProtArea);
	debugf("	lba48 = %u\n",drive->info.features.lba48);
	debugf("	lookAhead = %u\n",drive->info.features.lookAhead);
	debugf("	nop = %u\n",drive->info.features.nop);
	debugf("	packet = %u\n",drive->info.features.packet);
	debugf("	powerManagement = %u\n",drive->info.features.powerManagement);
	debugf("	powerupStandby = %u\n",drive->info.features.powerupStandby);
	debugf("	readBuffer = %u\n",drive->info.features.readBuffer);
	debugf("	releaseInt = %u\n",drive->info.features.releaseInt);
	debugf("	removableMedia = %u\n",drive->info.features.removableMedia);
	debugf("	removableMediaSN = %u\n",drive->info.features.removableMediaSN);
	debugf("	rwDMAQueued = %u\n",drive->info.features.rwDMAQueued);
	debugf("	securityMode = %u\n",drive->info.features.securityMode);
	debugf("	serviceInt = %u\n",drive->info.features.serviceInt);
	debugf("	setFeaturesSpinup = %u\n",drive->info.features.setFeaturesSpinup);
	debugf("	setMaxSecurity = %u\n",drive->info.features.setMaxSecurity);
	debugf("	smart = %u\n",drive->info.features.smart);
	debugf("	writeBuffer = %u\n",drive->info.features.writeBuffer);
	debugf("	writeCache = %u\n",drive->info.features.writeCache);
	debugf("\n");
}

#endif