/*	File:		PutAwayVolumes.c		Description:This code sample demonstrates several ways to unmount a volume.				[1]		Call MoreFiles and have it take its best shot.				[2]		Augment MoreFiles with a little Finder-style interface						and some strategic trash-emptying.				[3]		Send an AppleEvent to Finder and have it do the whole thing.								A real strategy might use all of the above. Basically, the problem is				this: although UnmountVol has been patched so that some commonly open				files (mostly having to do with desktop info) are closed before the				volume unmounts, not all are, and in particular files associated				with the trash on shared volumes are not automagically closed.						Each of the three above-mentioned methods has its flaws.						[1]		It wouldn't be in the spirit of MoreFiles to empty						the trash silently when attempting to unmount, so it's						not surprising MoreFiles doesn't do it.					[2]		Getting confirmation from the user involves posting an						alert, which is not always easy for non-apps.						[3]		Requires the unmounter to send AppleEvents, which is						not always easy for non-apps, and may require user to						switch to Finder to confirm alerts, which also is not						always easy for non-apps.						Your program may have to combine methods to achieve the effect it				needs. And your program should be ready for the methods to fail --				not that you should expect a proper implementation to fail, but				your program shouldn't assume that it will not.						Of course, if some application other than Finder or At Ease has files				open on the volume, the volume simply will not unmount, and that's a				legitimate error condition.					Note this sample in its current state will unmount all AppleShare				volumes. The interesting function is PutAwayOneVolume, and it has				nothing to do with AppleShare in particular. The rest of the functions				are here simply to drive PutAwayOneVolume.	Author:		PG	Copyright: 	Copyright: � 1996-1999 by Apple Computer, Inc.				all rights reserved.		Disclaimer:	You may incorporate this sample code into your applications without				restriction, though the sample code has been provided "AS IS" and the				responsibility for its operation is 100% yours.  However, what you are				not permitted to do is to redistribute the source as "DSC Sample Code"				after having made changes. If you're going to re-distribute the source,				we require that you make it clear in the source that the code was				descended from Apple Sample Code, but that you've made changes.		Change History (most recent first):				6/25/99		Updated for Metrowerks Codewarror Pro 2.1(KG)								03/20/96	St. Luther points out that 'hpbp->volumeParam.ioVFSID'							is not a sufficient test for AppleShare; check the driver							name instead; this makes the sample a little more accurate							but a little less useful, since nobody specifies volumes							by driver name. However, the portion of this code which							cares about which volumes to unmount is only here to make							the sample run anyway; users will have to adapt this code							regardless; PutAwayOneVolume is the interesting code, and							it's not affected by this change.(PG)					03/20/96	PutAwayVolumes doesn't really need to know the names of the							volumes it's indexing. Save some stack.(PG)					03/21/96	Use MoreFiles version of UnmountAndEject. Keeping my own							copy was just stupid, especially since I plan to use more							of MoreFiles.(PG)					03/21/96	After conversation with Jim, realized the AppleEvent							strategy is not always best. This version makes use of							semi-documented 'ReleaseFolder' call to deal with shared							trash folders.(PG)*/#define OLDROUTINELOCATIONS		0#define OLDROUTINENAMES			0#define SystemSevenOrLater		1#ifndef __FILES__#	include <Files.h>#endif#ifndef __LOWMEM__#	include <LowMem.h>#endif#ifndef __DEVICES__#	include <Devices.h>#endif#ifndef __PLSTRINGFUNCS__#	include <PLStringFuncs.h>#endif#ifndef __PROCESSES__#	include <Processes.h>#endif#ifndef __DIALOGS__#	include <Dialogs.h>#endif#include "PutAwayOneVolume.h"#define USE_INTERNAL_CODE 1////	USE_INTERNAL_CODE controls whether we send the AppleEvent to Finder//	or use the internal code to unmount the volume ('ReleaseFolder').//enum{	kCreatorCode_Finder = 'MACS',	kCreatorCode_AtEase = 'mfdr'};static pascal OSErr GetCreatorOfFinderLikeProcess (OSType *processSignature){	OSErr err = noErr;#if !USE_INTERNAL_CODE	ProcessSerialNumber		psn		= { kNoProcess, kNoProcess };	ProcessInfoRec			pir		= { sizeof (pir), nil };#endif	*processSignature = 0;#if !USE_INTERNAL_CODE	pir.processAppSpec = nil;	for (;;)	{		err = GetNextProcess (&psn);		if (err)		{			if (err == procNotFound) err = noErr;			break;		}		err = GetProcessInformation (&psn,&pir);		if (err) break;		if (pir.processSignature == kCreatorCode_Finder || pir.processSignature == kCreatorCode_AtEase)		{			*processSignature = pir.processSignature;			break;		}	}#endif	return err;}static pascal Boolean VolDriverNameIs (short vRefNum, ConstStr255Param potentialDriverName){	Boolean result = false;	const VCB *vcbQueueScanner = (const VCB *) GetVCBQHdr ( )->qHead;	while (vcbQueueScanner)	{		if (vcbQueueScanner->vcbVRefNum == vRefNum)		{			short csParam [11];			if (!Status (vcbQueueScanner->vcbDRefNum,1,csParam))			{				DCtlHandle			dCtlHandle		= *(DCtlHandle *) csParam;				DRVRHeaderPtr		drvrHeaderPtr	= (DRVRHeaderPtr) (**dCtlHandle).dCtlDriver;				Boolean				isHandle		= (**dCtlHandle).dCtlFlags & dRAMBasedMask;				ConstStr255Param	drvrName		= drvrHeaderPtr->drvrName;				SInt8				hState;				//				//	We lock the driver handle temporarily in case				//	<PLStringFuncs.glue.lib> (which contains				//	the implementation of PLstrcmp) lives in				//	another (potentially unloaded) code segment.				//				if (isHandle)				{					hState = HGetState ((Handle) drvrHeaderPtr);					HLock ((Handle) drvrHeaderPtr);					drvrName = (**(DRVRHeaderHandle)drvrHeaderPtr).drvrName;				}				result = !PLstrcmp (potentialDriverName,drvrName);				if (isHandle)					HSetState ((Handle) drvrHeaderPtr, hState);			}		}		vcbQueueScanner = (const VCB *) (vcbQueueScanner->qLink);	}	return result;}static pascal OSErr PutAwayVolumes (const unsigned char *driverName){	OSErr err = noErr;	OSType finderLikeProcess;	if (!(err = GetCreatorOfFinderLikeProcess (&finderLikeProcess)))	{		HParmBlkPtr hpbp = (HParmBlkPtr) NewPtrClear (sizeof (*hpbp));		if (!(err = MemError ( )))		{			hpbp->volumeParam.ioVolIndex = 1;				for (;;)			{				err = PBHGetVInfoSync (hpbp);				if (err)				{					if (err == nsvErr) err = noErr;					break;				}					if (VolDriverNameIs (hpbp->volumeParam.ioVRefNum, driverName))				{					err = PutAwayOneVolume (hpbp->volumeParam.ioVRefNum, finderLikeProcess);					if (err) break;				}					hpbp->volumeParam.ioVolIndex += 1;			}				DisposePtr ((Ptr) hpbp);			if (!err) err = MemError ( );		}	}	return err;}void main (void){	MaxApplZone ( );	InitGraf (&(qd.thePort));	InitFonts ( );	InitWindows ( );	InitMenus ( );	TEInit ( );	InitDialogs (nil);//	(void) PutAwayVolumes ("\p.Sony");	(void) PutAwayVolumes ("\p.AFPTranslator");}