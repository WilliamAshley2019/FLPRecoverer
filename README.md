 Currently rebuilding / debugging, optimistic this will work in the next day or so. 
Update pending later tonight early tomorrow, adding NTFS $MFT lookup to get index locations

- still need to determine raw parsing if NTFS $MFT etc.. is not enough (such as corrupted indexes) however that will come later.

- started a rebuild on it as for some reason fl studio and windows wasn't letting me execute the program couldn't find the bug and didn't feel like debugging so just built it back up using stage method.  the actual raw search method is still being developed,  the NTFS $MFT is a start to the raw approach.

TO DO.. consider creating an ability to freeze trim - this is more advanced so will be much later to add.

the ability to image the disk such as DD automation to an external source and work from the image file. 


There are a few bugs (such as file overwrites of same name) that should be updated in the next upload hopefully before I go to sleep today/tomorrow.
