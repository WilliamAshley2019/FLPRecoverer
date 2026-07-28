WARNING THIS IS EXPERIMENTAL WITH KNOWN BUGS!!!!!!!!!!!!!
2026-07-28 
It hasn't FUBARed my system yet testing so probably safe :)  Let me know, if you can, if you find any bugs   contact@williamashley.music
So the MFT search is working and can find deleted flp records.. however currently not sure how or if recovery is possible. As with trim even if the record is there the data likely isn't.
On a HDD or SSD with trim turned off this may be more forgiving.
However it works to discover the file name size and location of the file (was). This is progress. 
How does it work?  Click the button you want to do and wait for it to finish.

If it find stuff try select output folder - you need to use a real location not an alias, you need to select an actual real folder, placeholder paths will not work currently so like MUSIC, videos etc.. all the detail folders will not work. I suggest you make a folder and call it "flp-recovery" . Then you  click rcover all, currently I do not have a selectable toggle to pick and choose what to recover
TO DO  1. add selection toggle.

currently I am trying to determine if any magic is possible to unvanish trimmed data.


 2026-07-27
 Currently rebuilding / debugging, optimistic this will work in the next day or so. 
Update pending later tonight early tomorrow, adding NTFS $MFT lookup to get index locations

- still need to determine raw parsing if NTFS $MFT etc.. is not enough (such as corrupted indexes) however that will come later.

- started a rebuild on it as for some reason fl studio and windows wasn't letting me execute the program couldn't find the bug and didn't feel like debugging so just built it back up using stage method.  the actual raw search method is still being developed,  the NTFS $MFT is a start to the raw approach.        

TO DO.. consider creating an ability to freeze trim - this is more advanced so will be much later to add.

the ability to image the disk such as DD automation to an external source and work from the image file. 


There are a few bugs (such as file overwrites of same name) that should be updated in the next upload hopefully before I go to sleep today/tomorrow.


USE of flptoolkits  "data chunks"  for known binary string types to locate via an extensive parsed search for "flp fragments" and extract that data via flptoolkit to midi automation or other data that is already accessible via flptoolkit.
