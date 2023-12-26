# Directory-Upload-Program

## Needs
When you want to upload the directory, not just file, you need to implement it in other way with file uploading.
We often use this program, such as file-moving in local enviroment, Upload the directory on Cloud, etc.
Therefore, It is better to know the Directory file-uploading process as Software Engineer.

## Project Description
Basically, Client and Server are Multi-thread program. Client create a thread per file and the same goes for server. In this program, the most important point is Making Directory.
Server have to make the directroy structure that completely same with Client-sending directory structure. To implement this function, server always check that the directory is already created, if it is file or directory and how many thread server can create, etc.

This program provide debug function. You can see this fucntion in Makefile. You can debug it easily.

## Note
You need to observe closely the number of threads. Process make the threads unlimitedly, but it could be danger for server. I limited the number of thread as 1024. Therefore, You can send the 1024 files.
