WARNING THIS IS EXPERIMENTAL WITH KNOWN BUGS!!!!!!!!!!!!!
It hasn't FUBARed my system yet testing so probably safe :)  Let me know, if you can, if you find any bugs   contact@williamashley.music


2026-08-02 
Start of some instructions for recovery  - First if you think you have deleted something or will delete something turn off trim. If you have lots of space on your drive turn it off in advance of problems, read more about trim below, and do some reserach on it. The disable Trim toggle is an easy way to access it. 

Check FL Studio's own backup folder
Not built yet, but should be step zero: FL Studio's autosave/backup folder (if enabled) may just already have the file, no forensics needed at all. Fastest possible win when it applies.

Check Previous Versions (VSS)
Already built. Non-invasive, and when a snapshot exists it's typically a complete, unfragmented copy — the best possible outcome short of the file just existing.


Scan Directory
Already built. Covers "it's not deleted, just misplaced or the path was forgotten" — essentially free to try before assuming anything was actually lost.

Scan Deleted (NTFS)
Already built. If the file's genuinely gone but the filesystem itself is healthy, $MFT-based recovery gives you the real fragment map — far more reliable and faster than raw carving.


Image the drive, then scan the image
Already built (DD imaging + Scan Image File). The safe way to run the heavier stuff repeatedly without more risk to the original — work from a copy from here on.


Scan Physical Drive
For when the filesystem itself is damaged/reformatted/unreadable, or the $MFT record is gone. Slow, and the right tool only once everything above has failed or doesn't apply.



2026-08-01 - This is nearing a shelving point,



TO DO listing in the log of files discovered during scan and per line item functions,  the full drive scan functions need to be implemented again. I think a raw scan which will take a long time on a HDD should be done, I also need to sort out the raw scan on SSD which will likley be less useful unless Trim was disabled.

Need to make readme on how to use, particularly 1. disable trim   2. reenable trim if you have your file backup softawre working for flp files.   I sohuld make a better readme manual 
for what does what essentially volume shadow copy is just another layer of search the MFT is searching the current listing it may help identify files that were written but no longer there or help track down those files (likely won't exist for ssd files but may for hdd files) the drive, image and folder searches are essentially surface scans but might help find files that have been renamed to a non flp file type on advanced searches.

Likely atleast one more update until this is in a state I am happy with for the forseeable future as a basic concept for flp file recovery tool. Likely more to do with it. I should also maybe reskin it perhaps with the phosphor skin like unmp3, I sort of like that one.


2026-07-28 - added Trim detection and disable (havn't tested disabling function yet), added DD clone function, havn't tested yet. The logic here is that it still may be best practice
during recovery options to disable trim and create a clone so it made sense to add these two features into the App.

An Introduction on TRIM
The transition from magnetic to solid-state storage altered digital forensics and data recovery conditionerations in regard to the lifecycle of deleted data on NAND-based storage due to the TRIM function. In this explainer we'll detail how it effects recovery and steps to take to increase the chance of recovery. 
In the past deletion was a metadata operation, not a data operation. On traditional hard disk drives (HDDs), removing a file merely invalidated the filesystem's index entry. The underlying sectors retained their magnetic encoding until actively overwritten by new writes, or medium degredation. In essence the data was there even if the fs lookup was deleted until something else took that space.
The introduction of NAND flash memory introduced an abstraction layer between the operating system and the physical media. The TRIM command turned deletion operations into an active signal for irreversable physical destruction of the data not just the fs lookup. 

The Flash Translation Layer
An SSD presents a logical block address (LBA) interface to the host, identical in protocol to an HDD. Internally, however, the relationship between logical and physical addresses is governed by the Flash Translation Layer (FTL), a firmware-managed mapping database.
plain
Request: Write LBA 5000
        |
        v
FTL Mapping Table
        |
        v
Physical NAND Page (e.g., Page 8, Block 47)

The operating system doesn't natively track the physical location it uses a FS Lookup. The controller alone maintains the LBA-to-physical-page mapping.
NAND Write Constraints
  NAND flash cannot be overwritten in place. NAND flash memory operates at the page level for reads and writes, but at the block level for erasures. A typical contemporary layout might use 16 KB pages grouped into 2 MB erase blocks. Programming (writing) can only change cells in one direction; to modify even a single byte, the entire containing block must be erased and rewritten elsewhere.

Operation          |  Granularity
-------------------|------------------
Read               |  Page (16 KB)
Program (Write)    |  Page (16 KB)
Erase              |  Block (2 MB)

This asymmetry means that every logical overwrite becomes, at the physical level, a write to a new page and the invalidation of the old one. The FTL must track which physical pages are stale and which are valid.

Without a mechanism to distinguish valid data from discarded data, the SSD controller must preserve every physical page that was ever written. Eventually, all blocks become fully populated with a mixture of valid and stale pages. When the host requests a write and no empty blocks exist, the controller must perform garbage collection: reading valid pages from a target block, writing them to a new block, and erasing the original block. This process is slow and causes severe write amplification.

TRIM was introduced so that when the operating system deletes a file, it issues a TRIM command (ATA TRIM for SATA; Dataset Management / Deallocate for NVMe) to inform the controller that specific LBAs no longer contain meaningful data.

Host to Controller:  "LBAs 40000 through 70000 are no longer in use."
The controller updates its internal metadata to mark the corresponding physical pages as invalid. No erasure occurs at this moment. The data remains physically encoded in the floating-gate or charge-trap cells. However, the controller is now explicitly authorized to destroy those pages during subsequent garbage collection. The actual destruction is performed by garbage collection, which may occur seconds, minutes, or hours later depending on drive utilization, idle time, and controller algorithms. The moment of data death is not the TRIM command itself, but the subsequent erase cycle triggered by the controller's autonomous maintenance routines which can be dynamic depending on drive usage.


Consider a NAND block containing four pages:

Block N
---------
Page A  |  Valid data (File: Photo.jpg)
Page B  |  Valid data (File: Document.docx)
Page C  |  Valid data (File: Music.mp3)
Page D  |  Valid data (File: Movie.mp4)
After the user deletes Photo.jpg and TRIM is issued, the controller updates its mapping:

Block N
---------
Page A  |  INVALID (was Photo.jpg)
Page B  |  Valid
Page C  |  Valid
Page D  |  Valid

If the controller requires free space, it selects Block N for garbage collection. It copies Pages B and C to a new block, then erases Block N entirely. Pages A and D (the invalid page and any co-located pages) are destroyed by the erase cycle. The charge phyiscally holding the data on the hardware is drained from the floating gates. Based on currently available technology there is no way to access the gates and determine past data writes. AFAIK.

This creates a narrow, unpredictable recovery window:

Deletion + TRIM
      |
      |----> Data still physically exists
      |
      |----> Garbage collection executes
      |
      v
Data erased
The duration of this window depends on:
- Drive write activity (host-generated writes)
- Background controller tasks
- Available spare area (over-provisioning)
- Power state transitions
Under heavy use, the window may close within minutes. On an idle drive, it may remain open for hours or days. The forensic practitioner cannot know which case applies.

Disabling TRIM does not recover data. It preserves the recovery window by preventing the operating system from sending new deallocation signals to the controller. Once TRIM is disabled, subsequent deletions are treated by the controller as indeterminate: the pages remain marked as valid in the FTL because the controller lacks explicit authorization to invalidate them.

This is crucial in live-system scenarios where the user continues to operate the computer while attempting recovery. Without disabling TRIM, every new file deletion, cache clear, or temporary file removal sends additional TRIM commands, expanding the set of pages marked for garbage collection and accelerating the destruction of the target data.


A powered-on Windows system generates continuous background writes:
- Browser cache and cookies
- Windows Update staging
- Search indexing
- Antivirus scanning
- Pagefile and hibernation file activity
- Event logging
- Superfetch / SysMain

Each of these writes may trigger garbage collection. Each garbage collection cycle may reclaim trimmed pages. The probability of recovery drops exponentially with system uptime following deletion.

The ideal response to accidental deletion on an SSD is immediate power-off. This halts all host writes and all controller background operations. The drive should then be connected to a write-blocked forensic workstation, imaged at the logical level, and recovery attempted from the image—not the original drive. 

Professional forensic laboratories image first and analyze second precisely because every power-on event provides the controller with an opportunity to execute deferred garbage collection.

The ATA specification defines the TRIM command as part of the DATA SET MANAGEMENT feature set. The host sends a bitmap or range of LBAs to be trimmed. The drive responds with command acceptance and processes the deallocation asynchronously.


NVMe uses the Dataset Management command with the Attribute Deallocate (AD) bit set. This is functionally equivalent to ATA TRIM but operates over the PCIe bus with lower latency and higher throughput. The semantic effect—informing the controller that LBAs are disposable—is identical.

Enterprise and some consumer drives implement deterministic read-after-TRIM behavior:
- Deterministic Zero After Trim (DZAT): The controller returns zeroes for trimmed LBAs immediately, even before physical erasure.
- Deterministic Read After Trim (DRAT): The controller returns deterministic but non-zero data.
In both cases, the original data is hidden from the host interface the instant TRIM is acknowledged. Recovery through logical imaging becomes impossible immediately, though chip-off forensics might still retrieve the physical pages if they have not yet been erased.


Modern SSDs continuously relocate static data to distribute program/erase cycles evenly across the NAND array. A logical block may reside at Physical Page 812 today, Physical Page 9014 tomorrow, and Physical Page 212 next week. Only the controller's volatile or semi-persistent mapping table knows the current location.
This has two consequences for recovery:
1. Physical page addresses have no stable relationship to logical file offsets.
2. Chip-off recovery (removing NAND chips and reading them directly) is often futile without the mapping table, which is stored in the controller's RAM or internal NAND and may be encrypted.

Wear leveling thus compounds the TRIM problem: even if the target pages have not yet been erased, finding them without the controller's cooperation is computationally intractable.

Secure Erase: A Distinct and Final Operation
TRIM must not be confused with Secure Erase. TRIM marks specific pages as disposable. Secure Erase initiates a drive-wide sanitization process, typically by discarding the media encryption key (if the drive uses self-encryption) or by scheduling every block for erasure. Recovery after Secure Erase is generally considered impossible by any practical means.

Immediate Response Protocol

Step 1:  Stop all write activity to the affected drive.
Step 2:  If the system is running, disable TRIM immediately.
        Windows:  fsutil behavior set DisableDeleteNotify 1  (this is what the app does when you disable Trim via the App)
        Linux:    mount -o discard=none /dev/sdX
        macOS:    sudo trimforce disable
Step 3:  Power down the system gracefully.
Step 4:  Remove the SSD and connect to a write-blocked imaging station.
Step 5:  Create a verified bit-for-bit image.
Step 6:  Perform all recovery operations on the image, never the original.

Verification
Before imaging, verify that TRIM is disabled:   (I think this is correctly displaying Trim status have not deeply tested)

Windows:  fsutil behavior query DisableDeleteNotify
          Expected: DisableDeleteNotify = 1


Disabling TRIM on a live system prevents future TRIM commands but cannot retract TRIM commands already issued. If the deletion occurred hours ago and the system remained active, the data is likely already gone. Disabling TRIM is a preservation measure for the remaining recoverable data, not a time machine.

How NAND Works 
NAND flash stores data as electrical charge trapped in insulated floating gates (legacy planar NAND) or charge-trap layers (modern 3D NAND). Reading measures threshold voltage; programming injects electrons; erasing removes them from an entire block via quantum tunneling.
Once a block is erased, the charge distribution is reset. There is no residual magnetic remanence analogous to HDD platter forensics. The information is destroyed at the level of electron population statistics. No current technology can reconstruct the previous charge state of a erased NAND cell. This physical finality is why software cannot recover data that has been garbage-collected: the information literally no longer exists in the storage medium. AFAIK

Summary TRIM must be disabled at the earliest possible moment, the drive must be imaged before analysis, and recovery expectations must be calibrated against the reality that SSDs are active information-management systems, not passive archival media. The window for recovery is narrow, contingent, and closing from the instant the delete key is pressed.
The TRIM button lets you disable TRIM, while the DD button is intended to create a backup to an external or secondary drive to work from the disk image preserving its state as well as possible.  Obviously this is intended for people who will not send their ssd to a professional recovery service to recover their FLP files. The sad thing is generally speaking if they are on SSD it is very difficult to recover them if not done immediately.

- ATA/ATAPI Command Set - 4 (ACS-4), INCITS 529-2018
- NVM Express Base Specification, Revision 2.0, NVM Express Inc.
- Fowler, K. et al. "Solid-State Drive Forensics: Where We Stand."
  Digital Investigation, 2019.
- Breeuwsma, M. "Forensic Data Recovery from Flash Memory."
  Small Scale Digital Device Forensics Journal, 2006.
- Micron Technical Note TN-29-17: "NAND Flash 101"
- ISO/IEC 27037:2012 - Guidelines for identification, collection,
  acquisition and preservation of digital evidence

2026-07-28 
It hasn't FUBARed my system yet testing so probably safe :)  Let me know, if you can, if you find any bugs   contact@williamashley.music
So the MFT search is working and can find deleted flp records.. however currently not sure how or if recovery is possible. As with trim even if the record is there the data likely isn't.
On a HDD or SSD with trim turned off this may be more forgiving.
However it works to discover the file name size and location of the file (was). This is progress. 
How does it work?  Click the button you want to do and wait for it to finish.

If it find stuff try select output folder - you need to use a real location not an alias, you need to select an actual real folder, placeholder paths will not work currently so like MUSIC, videos etc.. all the detail folders will not work. I suggest you make a folder and call it "flp-recovery" . Then you  click rcover all, currently I do not have a selectable toggle to pick and choose what to recover
TO DO  1. add selection toggle.

currently I am trying to determine if any magic is possible to unvanish trimmed data.

note in the current version a file may recover successfully but be filled with NUL if nothing is at the location of recovery, this is not precognative shaedenfrued it is
just an attempt at recovery.

 2026-07-27
 Currently rebuilding / debugging, optimistic this will work in the next day or so. 
Update pending later tonight early tomorrow, adding NTFS $MFT lookup to get index locations

- still need to determine raw parsing if NTFS $MFT etc.. is not enough (such as corrupted indexes) however that will come later.

- started a rebuild on it as for some reason fl studio and windows wasn't letting me execute the program couldn't find the bug and didn't feel like debugging so just built it back up using stage method.  the actual raw search method is still being developed,  the NTFS $MFT is a start to the raw approach.        

TO DO.. consider creating an ability to freeze trim - this is more advanced so will be much later to add.

the ability to image the disk such as DD automation to an external source and work from the image file. 


There are a few bugs (such as file overwrites of same name) that should be updated in the next upload hopefully before I go to sleep today/tomorrow.


USE of flptoolkits  "data chunks"  for known binary string types to locate via an extensive parsed search for "flp fragments" and extract that data via flptoolkit to midi automation or other data that is already accessible via flptoolkit.
