//============================================================================
// Author      : Guoqing Xiong
// Version     :
// Copyright   : Your copyright notice
// Description : file system
//============================================================================
#define FUSE_USE_VERSION 27
#define BLOCKSIZE 4096
#define TOTALFILE 10000
#define MAXSIZE 1638400
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>

char* currWorkingDir = "/";
const int ROOT_DIRECTORY_INODE = 26;
const char* FILE_NAME_PREFIX = "/fusedata/fusedata.";

typedef struct super_block_struct {
	int creationTime;
	int mounted;
	int dev_id;
	int freeStat;
	int freeEnd;
	int root;
	int maxBlocks;
} superBlock;

typedef struct array {
	int nums[400];
} arr;

typedef struct inode_block {
	int size;
	int uid;
	int gid;
	int mode;
	int atime;
	int ctime;
	int mtime;
	int linkcount;
	int indirect;
	int valid;
	char content[400][255];
	int location[400];
	int lastWriteLocation;
} inodeBlock;

int openedFileMapValid = 0;
int openedFileMapiNodes[5000];
char openedFileMapTruePath[5000][255];

int pathToInodeMapValid = 0;
int pathToInodeMapiNodes[5000];
char pathToInodeMapTruePath[5000][255];

int fileInfoMapValid = 0;
int fileInfoMapPosition[5000];
int fileInfoMapBlocks[5000];
int fileInfoMapiNode[5000];
int fileInfoMapFlags[5000];

typedef struct singleInfo {
	int position;
	int blocks;
	int inode;
	int flags;
} singleFileInfo;
/***************************helper functions***************************/
//complete,get true size of the int array
int getBlockSize(int* blockList) {
	int size = 0;
	int i;
	for (i = 0; i < 400; i++) {
		if (blockList[i] == 0) {
			break;
		}
		size++;
	}
	return size;
}

//read all the content in the file
char* readAllFile(char* fileName) {
	FILE *fp;
	fp = fopen(fileName, "r");
	char* fcontent = malloc(sizeof(char) * BLOCKSIZE);
	fread(fcontent, 1, BLOCKSIZE, fp);
	return fcontent;
}

//transfer string to integer
int stringToInt(char* num) {
	char *contentEnd;
	int number = strtol(num, &contentEnd, 10);
	return number;
}

//transfer integer to string
char* intToString(int num) {
	char *tmp = malloc(sizeof(char) * 10);
	sprintf(tmp, "%d", num);
	return tmp;
}

//get substring of a string starts at (start) with length (length)
//from stackoverflow
char* substring(int start, int length, char* ori) {
	char* original = malloc(sizeof(ori));
	strcpy(original, ori);
	char *subbuff = malloc(sizeof(char) * (length + 1));
	memcpy(subbuff, &original[start], length);
	subbuff[length] = '\0';
	return subbuff;
}
//end

//check if a path is in fastInodeLookUpTable
int findInPathToInodeMap(char* path) {
	int i;
	for (i = 0; i < pathToInodeMapValid; i++) {
		if (strcmp(pathToInodeMapTruePath[i], path) == 0) {
			return 0;
		}
	}
	return -1;
}

//get path related inode fom fastInodeLookUpTable
int getInPathToInodeMap(char* path) {
	int i;
	for (i = 0; i < pathToInodeMapValid; i++) {
		if (strcmp(pathToInodeMapTruePath[i], path) == 0) {
			return pathToInodeMapiNodes[i];
		}
	}
	return 0;
}

//delete the path fom fastInodeLookUpTable
void deleteInPathToInodeMap(char *path) {
	int i;
	int find = -1;
	for (i = 0; i < pathToInodeMapValid; i++) {
		if (strcmp(pathToInodeMapTruePath[i], path) == 0) {
			find = i;
			break;
		}
	}
	if (find == -1) {
		return;
	}
	for (i = find; i < pathToInodeMapValid; i++) {
		strcpy(pathToInodeMapTruePath[i], pathToInodeMapTruePath[i + 1]);
	}
	for (i = find; i < pathToInodeMapValid; i++) {
		pathToInodeMapiNodes[i] = pathToInodeMapiNodes[i + 1];
	}
	pathToInodeMapValid = pathToInodeMapValid - 1;
}

//intert (path,inode) into fastInodeLookUpTable
void insertIntoPathToInodeMap(char *path, int inode) {
//	int i;
	if (findInPathToInodeMap(path) != -1) {
		return;
	}
//problem
//	for (i = 0; i < pathToInodeMapValid; i++) {
//		if (pathToInodeMapiNodes[i] == inode) {
//			strcpy(pathToInodeMapTruePath[i], path);
//			return;
//		}
//	}
	pathToInodeMapiNodes[pathToInodeMapValid] = inode;
	strcpy(pathToInodeMapTruePath[pathToInodeMapValid], path);
	pathToInodeMapValid++;
}

//find if this inode is in opened file map
int findInOpenedFileMap(int inode) {
	int i;
	for (i = 0; i < openedFileMapValid; i++) {
		if (openedFileMapiNodes[i] == inode) {
			return 0;
		}
	}
	return -1;
}

//get related path in opened file map by inode
char* getInOpenedFileMap(int inode) {
	int i;
	char *result = malloc(sizeof(char) * 255);
	for (i = 0; i < openedFileMapValid; i++) {
		if (openedFileMapiNodes[i] == inode) {
			strcpy(result, openedFileMapTruePath[i]);
			return result;
		}
	}
	return result;
}

//delete this inode in opened file map
void deleteInOpenedFileMap(int inode) {
	int i;
	int find = -1;
	for (i = 0; i < openedFileMapValid; i++) {
		if (openedFileMapiNodes[i] == inode) {
			find = i;
			break;
		}
	}
	if (find == -1) {
		return;
	}
	for (i = find; i < openedFileMapValid; i++) {
		strcpy(openedFileMapTruePath[i], openedFileMapTruePath[i + 1]);
	}
	for (i = find; i < openedFileMapValid; i++) {
		openedFileMapiNodes[i] = openedFileMapiNodes[i + 1];
	}
	openedFileMapValid = openedFileMapValid - 1;
}

//insert (key,value) into opened file map
void insertIntoOpenedFileMap(int key, char* value) {
	int i;
	for (i = 0; i < openedFileMapValid; i++) {
		if (openedFileMapiNodes[i] == key) {
			strcpy(openedFileMapTruePath[i], value);
			return;
		}
	}
	openedFileMapiNodes[openedFileMapValid] = key;
	strcpy(openedFileMapTruePath[openedFileMapValid], value);
	openedFileMapValid = openedFileMapValid + 1;
}

//check if this inode is in file info map
int findInFileInfoMap(int inode) {
	int i;
	for (i = 0; i < fileInfoMapValid; i++) {
		if (fileInfoMapiNode[i] == inode) {
			return 0;
		}
	}
	return -1;
}

//get file info from file info map by inode
singleFileInfo* getInFileInfoMap(int inode) {
	int i;
	singleFileInfo *result = malloc(sizeof(singleFileInfo));
	for (i = 0; i < fileInfoMapValid; i++) {
		if (fileInfoMapiNode[i] == inode) {
			result->blocks = fileInfoMapBlocks[i];
			result->flags = fileInfoMapFlags[i];
			result->inode = inode;
			result->position = fileInfoMapPosition[i];
			return result;
		}
	}
	result->blocks = -1;
	result->flags = -1;
	result->inode = -1;
	result->position = -1;
	return result;
}

//delete this inode in file info map
void deleteInFielInfoMap(int inode) {
	int i;
	int find = -1;
	for (i = 0; i < fileInfoMapValid; i++) {
		if (fileInfoMapiNode[i] == inode) {
			find = i;
			break;
		}
	}
	if (find == -1) {
		return;
	}
	for (i = find; i < fileInfoMapValid; i++) {
		fileInfoMapiNode[i] = fileInfoMapiNode[i + 1];
		fileInfoMapBlocks[i] = fileInfoMapBlocks[i + 1];
		fileInfoMapFlags[i] = fileInfoMapFlags[i + 1];
		fileInfoMapPosition[i] = fileInfoMapPosition[i + 1];
	}
	fileInfoMapValid = fileInfoMapValid - 1;
}

//insert into file info map
void insertIntoFileInfoMap(int key, int position, int block, int flags) {
	int i;
	for (i = 0; i < fileInfoMapValid; i++) {
		if (fileInfoMapiNode[i] == key) {
			fileInfoMapBlocks[i] = block;
			fileInfoMapFlags[i] = flags;
			fileInfoMapiNode[i] = key;
			fileInfoMapPosition[i] = position;
			return;
		}
	}
	fileInfoMapBlocks[i] = block;
	fileInfoMapFlags[i] = flags;
	fileInfoMapiNode[i] = key;
	fileInfoMapPosition[i] = position;
	fileInfoMapValid = fileInfoMapValid + 1;
}

//insert inode location and path into a inode block
void insertIntoInodeBlock(inodeBlock *inode, char* path, int num) {
	int valid = inode->valid;
	inode->location[valid] = num;
	strcpy(inode->content[valid], path);
	inode->valid = inode->valid + 1;
}

//delete the path in inode block
void deleteInInodeBlock(inodeBlock *inode, char* path) {
	int i;
	int valid = inode->valid;
	int location = -1;
	for (i = 0; i < valid; i++) {
		if (strcmp(path, inode->content[i]) == 0) {
			location = i;
			break;
		}
	}
	if (location != -1) {
		for (i = location; i < valid; i++) {
			strcpy(inode->content[i], inode->content[i + 1]);
			inode->location[i] = inode->location[i + 1];
		}
		inode->valid = inode->valid - 1;
	}
}

//put number from start to end to a int*
int* generateNumList(int start, int end) {
	int *data = malloc(sizeof(int) * 400);
	int i;
	int count = 0;
	for (i = start; i < end; i++) {
		data[count] = i;
		count++;
	}
	for (; count < 401; count++) {
		data[count] = 0;
		count++;
	}
	return data;
}

//concatenate new to old, like old+new in cpp
char* getCat(char* old, char* new) {
	char *tmp = malloc(sizeof(char) * 255);
	strcpy(tmp, old);
	strcat(tmp, new);
	return tmp;
}

//get absolute path from a path
char* getAbsolutePath(char* aPath) {
	char *path = malloc(sizeof(aPath));
	strcpy(path, aPath);
	if (strcmp(path, "") == 0) {
		return path;
	}
	char *result = malloc(sizeof(char) * 255);
	int start = 0;
	int end = 0;
	int stackSize = 0;
	char stack[255][255];
	strcpy(stack[0], "/");
	stackSize++;
	int length = strlen(path);
	while (start < length) {
		char *cmpSub = malloc(sizeof(char) * 255);
		if (start < length) {
			char *tmp = substring(start, 1, path);
			strcpy(cmpSub, tmp);
//			free(tmp);
		}
		while ((start < length) && (strcmp(cmpSub, "/") == 0)) {
			start++;
			char *tmp = substring(start, 1, path);
			strcpy(cmpSub, tmp);
//			free(tmp);
		}
		end = start;
		if (end < length) {
			char *tmp = substring(end, 1, path);
			strcpy(cmpSub, tmp);
//			free(tmp);
		}
		while ((end < length) && (strcmp(cmpSub, "/") != 0)) {
			end++;
			char *tmp = substring(end, 1, path);
			strcpy(cmpSub, tmp);
//			free(tmp);
		}
		char *each = malloc(sizeof(char) * 255);
		strcpy(each, substring(start, end - start, path));
		if (strcmp(each, "..") == 0) {
			if (strcmp(stack[stackSize - 1], "/") != 0) {
				stackSize--;
			}
		} else if ((strcmp(each, ".") != 0) && (strcmp(each, "") != 0)) {
			strcpy(stack[stackSize], each);
			stackSize++;
		}
		start = end;
//		free(cmpSub);
	}
	if (stackSize == 1) {
		char *tmp = malloc(sizeof(char) * 255);
		strcpy(tmp, stack[0]);
		return tmp;
	}
	int justStart = 0;
	int counter = 0;
	while (counter < stackSize) {
		if (justStart == 0) {
			strcpy(result, stack[0]);
			justStart++;
			counter++;
			continue;
		}
		strcat(result, stack[counter]);
		strcat(result, "/");
		counter++;
	}
	strcpy(result, substring(0, strlen(result) - 1, result));
	return result;
}

//get absolute parent path from a path
char* getAbsParentPath(char* path) {
	return getAbsolutePath(getCat(path, "/.."));
}

//get the last char* in a splitted char* array
char* getLastOneinString(char** tokens) {
	char *tmp = malloc(sizeof(char) * 20);
	if (tokens) {
		int i;
		for (i = 0; *(tokens + i); i++) {
			strcpy(tmp, *(tokens + i));
//			free(*(tokens + i));
		}
		free(tokens);
	}
	return tmp;
}

//string split,from stackoverflow
char** splitString(char* a_str, const char a_delim) {
	char** result = 0;
	size_t count = 0;
	char* tmp = a_str;
	char* last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;
	/* Count how many elements will be extracted. */
	while (*tmp) {
		if (a_delim == *tmp) {
			count++;
			last_comma = tmp;
		}
		tmp++;
	}
	/* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);
	/* Add space for terminating null string so caller
	 knows where the list of returned strings ends. */
	count++;
	result = malloc(sizeof(char*) * count);
	if (result) {
		size_t idx = 0;
		char* token = strtok(a_str, delim);

		while (token) {
			*(result + idx++) = strdup(token);
			token = strtok(0, delim);
		}
		*(result + idx) = 0;
	}
	return result;
}
//end

//generate file name from given number->fuseData.num
char* getFileName(int num) {
	char *tmp = malloc(sizeof(char) * 255);
	sprintf(tmp, "%d", num);
	strcpy(tmp, FILE_NAME_PREFIX);
	char *fileNum = intToString(num);
	strcat(tmp, fileNum);
	free(fileNum);
	return tmp;
}

//get information of super block
superBlock* getSuperBlockInfoData() {
	char* tmp = getFileName(0);
	superBlock *object = malloc(sizeof(superBlock));
	FILE * file = fopen(tmp, "rb");
	if (file != NULL) {
		fread(object, sizeof(superBlock), 1, file);
		fclose(file);
	}
	free(tmp);
	return object;
}

//get information of an array
arr* getArrayData(int fileNumber) {
	char* tmp = getFileName(fileNumber);
	arr *object = malloc(sizeof(arr));
	FILE * file = fopen(tmp, "rb");
	if (file != NULL) {
		fread(object, sizeof(arr), 1, file);
		fclose(file);
	}
	free(tmp);
	return object;
}

//get infomation of an inode block
inodeBlock* getInodeInfoData(int fileNumber) {
	char* tmp = getFileName(fileNumber);
	inodeBlock *object = malloc(sizeof(inodeBlock));
	FILE * file = fopen(tmp, "rb");
	if (file != NULL) {
		fread(object, sizeof(inodeBlock), 1, file);
		fclose(file);
	}
	free(tmp);
	return object;
}

//write super block to file
void writeSuperBlocktoBlock(superBlock *superB) {
	char *tmp = getFileName(0);
	FILE * file = fopen(tmp, "wb");
	if (file != NULL) {
		fwrite(superB, sizeof(superBlock), 1, file);
		fclose(file);
	}
	free(tmp);
}

//write inode block to file
void writeInodeToBlock(int fileNumber, inodeBlock *fileInode) {
	char *tmp = getFileName(fileNumber);
	FILE * file = fopen(tmp, "wb");
	if (file != NULL) {
		fwrite(fileInode, sizeof(inodeBlock), 1, file);
		fclose(file);
	}
	free(tmp);
}

//write array to file
void writeArrayToBlock(int fileNumber, arr *numList) {
	char *tmp = getFileName(fileNumber);
	FILE * file = fopen(tmp, "wb");
	if (file != NULL) {
		fwrite(numList, sizeof(arr), 1, file);
		fclose(file);
	}
	free(tmp);
}

//create a file with name fuseData.fileNumber and BLOCKSIZE "0" inside
void createFile(int fileNumber) {
	char* tmp;
	tmp = getFileName(fileNumber);
	FILE *p = fopen(tmp, "w");
	int i;
	for (i = 0; i < BLOCKSIZE; i++) {
		fwrite("0", 1, 1, p);
	}
	fclose(p);
	free(tmp);
}

//remove all file
void removeAllFile() {
	int i;
	for (i = 0; i < TOTALFILE; i++) {
		char* tmp = getFileName(i);
		remove(tmp);
		free(tmp);
	}
}

//descending order
int compare(const void* a, const void* b) {
	int int_a = *((int*) a);
	int int_b = *((int*) b);

	if (int_a == int_b)
		return 0;
	else if (int_a > int_b)
		return -1;
	else
		return 1;
}

//find a free block
int findFreeBlock() {
	int i;
	for (i = 1; i < 26; i++) {
		arr *blockList = getArrayData(i);
		qsort(blockList->nums, 400, sizeof(int), compare);
		int freeBlock = blockList->nums[0];
		if (freeBlock == 0) {
			free(blockList);
			continue;
		}
		blockList->nums[0] = 0;
		qsort(blockList->nums, 400, sizeof(int), compare);
		writeArrayToBlock(i, blockList);
		free(blockList);
		return freeBlock;
	}
	return -1;
}

//find multiple free blocks
int* findMultiFreeBlocks(int num) {
	int *result = malloc(sizeof(int) * 400);
	memset(result, 0, sizeof(int) * 400);
	if (num == 0) {
		return result;
	}
	int i;
	int curr = 0;
	for (i = 1; i < 26; i++) {
		arr *tmpBlockList = getArrayData(i);
		qsort(tmpBlockList->nums, 400, sizeof(int), compare);
		int counter = 0;
		while (tmpBlockList->nums[counter] != 0) {
			result[curr] = tmpBlockList->nums[counter];
			tmpBlockList->nums[counter] = 0;
			counter++;
			curr++;
			if (curr == num) {
				qsort(tmpBlockList->nums, 400, sizeof(int), compare);
				writeArrayToBlock(i, tmpBlockList);
				free(tmpBlockList);
				return result;
			}
			qsort(tmpBlockList->nums, 400, sizeof(int), compare);
			writeArrayToBlock(i, tmpBlockList);
		}
		free(tmpBlockList);
	}
	return result;
}

//find the position of key in an array
int findInList(int* nums, int key) {
	int i;
	for (i = 0; i < sizeof(nums) / sizeof(int); i++) {
		if (nums[i] == key) {
			return i;
		}
	}
	return -1;
}
void eraseFile(int blockNum) {
//	printf("erased block number is:%d\n", blockNum);
	char* fileName = getFileName(blockNum);
	FILE *fp = fopen(fileName, "w");
	fclose(fp);
	free(fileName);
}
//add one block to the free block list
void addOneToFreeBlock(int blockNumber) {
	arr* data = getArrayData(blockNumber / 400 + 1);
	qsort(data->nums, 400, sizeof(int), compare);
	int size = getBlockSize(data->nums);
	data->nums[size] = blockNumber;
	qsort(data->nums, 400, sizeof(int), compare);
	writeArrayToBlock(blockNumber / 400 + 1, data);
	eraseFile(blockNumber);
	free(data);
}

//add more than one blocks to the free block list
void addMultipleToFreeBlock(int* blockList) {
	int size = getBlockSize(blockList);
	int i;
	for (i = 0; i < size; i++) {
		addOneToFreeBlock(blockList[i]);
	}
}

/***************************file system functions***************************/
//erase the content of the file
//TRUNCATE
int fs_truncate(const char *path, off_t offset) {
//test use
//	arr* tmpData = getArrayData(1);
//	int tmpSize = getBlockSize(tmpData->nums);
//	printf("%d\n", tmpSize);
	//bad address
	if (strcmp(path, "") == 0) {
		return -EFAULT;
	}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	int iNode = getInPathToInodeMap(truePath);
	if (findInOpenedFileMap(iNode) != -1) {
		//means close the file
		deleteInOpenedFileMap(iNode);
	}
	inodeBlock *fileInodeInfo = getInodeInfoData(iNode);
	fileInodeInfo->size = 0;
	if (fileInodeInfo->valid < 0) {
		insertIntoOpenedFileMap(iNode, getFileName(iNode));
	}
//	printf("in truncate\n");
//	printf("the file inode valid is:%d\n", fileInodeInfo->valid);
//	printf("the indirect is:%d\n", fileInodeInfo->indirect);
	if (fileInodeInfo->valid == -2 && fileInodeInfo->indirect == 0) {
//		printf("%d\n",fileInodeInfo->location[0]);
		addOneToFreeBlock(fileInodeInfo->location[0]);
		fileInodeInfo->valid = -1;
	} else if (fileInodeInfo->indirect != 0 && fileInodeInfo->valid == -2) {
//		printf("enter success in second loop\n");
		int* blockList = getArrayData(fileInodeInfo->location[0])->nums;
		addOneToFreeBlock(fileInodeInfo->location[0]);
		fileInodeInfo->valid = -1;
		fileInodeInfo->indirect = 0;
//		printf("location zero is:%d\n", fileInodeInfo->location[0]);
//		int blockListSize = getBlockSize(blockList);
//		int i;
//		for (i = 0; i < 10; i++) {
//			printf("curr is:%d\n", blockList[i]);
//		}
		addMultipleToFreeBlock(blockList);
		free(blockList);
	}
//	printf("end truncate\n");
	fileInodeInfo->valid = -1;
//	fileInodeInfo->location[0] = -1;
	writeInodeToBlock(iNode, fileInodeInfo);
	insertIntoOpenedFileMap(iNode, getFileName(iNode));
	insertIntoFileInfoMap(iNode, 0, iNode, 0);
	free(fileInodeInfo);
	free(aPath);
	free(truePath);
//test use
//	arr* tmpData1 = getArrayData(1);
//	tmpSize = getBlockSize(tmpData1->nums);
//	printf("%d\n", tmpSize);
	return 0;
}
//write helper function
int appendToFile(char* content, inodeBlock* infoMap) {
	int count = strlen(content);
	int totalFileSize = infoMap->size;
	infoMap->valid = -2;
//	printf("last write is:%d\n",infoMap->lastWriteLocation);
//	printf("in append before count is:%d\n", count);
//	printf("in append before file size is:%d\n", totalFileSize);
//	printf("totalFileSize is:%d\n",totalFileSize);
//	printf("count is:%d\n",count);
//	printf("(totalFileSize % BLOCKSIZE) + count=%d\n",(totalFileSize%BLOCKSIZE)+count);
	//first condition right
	if (totalFileSize < BLOCKSIZE && totalFileSize + count <= BLOCKSIZE) {
//		printf("1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st 1st \n");
		char* fileName = getFileName(infoMap->location[0]);
		FILE *fp = fopen(fileName, "a+");
		fwrite(content, sizeof(char), count, fp);
		fclose(fp);
		free(fileName);
		//return last write location
		return infoMap->location[0];
	}
	if (totalFileSize > BLOCKSIZE
			&& (totalFileSize % BLOCKSIZE) + count <= BLOCKSIZE) {
//		printf("2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd 2nd \n");
		if (totalFileSize % BLOCKSIZE == 0) {
			int aNewBlock = findFreeBlock();
			eraseFile(aNewBlock);
			int contentLen = (int) strlen(content);
			char tmpContent[contentLen];
			strcpy(tmpContent, content);
			arr* usedBlocks = getArrayData(infoMap->location[0]);
			int usedSize = getBlockSize(usedBlocks->nums);
			usedBlocks->nums[usedSize] = aNewBlock;
			writeArrayToBlock(infoMap->location[0], usedBlocks);
			free(usedBlocks);
			char* fileName = getFileName(aNewBlock);
			FILE *fp = fopen(fileName, "a+");
			fwrite(tmpContent, sizeof(char), count, fp);
			fclose(fp);
			free(fileName);
			return aNewBlock;
		} else {
			//no use here
			int lastWrite = infoMap->lastWriteLocation;
//		printf("last write locatio is:%d\n",lastWrite);
			char* fileName = getFileName(lastWrite);
			FILE *fp = fopen(fileName, "a+");
			fwrite(content, sizeof(char), count, fp);
			fclose(fp);
			free(fileName);
			return lastWrite;
		}
	}
	//third condition right
	if (totalFileSize <= BLOCKSIZE && totalFileSize + count > BLOCKSIZE) {
		infoMap->indirect = 1;
//		printf("3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd 3rd \n");
		//ready to establish an index block
		int indexBlock = findFreeBlock();
		eraseFile(indexBlock);
//		printf("index block find is:%d\n", indexBlock);
		//calculate additional blocks needed
		int neededBlock = ((count - (BLOCKSIZE - totalFileSize)) / BLOCKSIZE)
				+ 1;
//		printf("addition block needed:%d\n", neededBlock);
		//make up original file
		int contentLen = (int) strlen(content);
		char tmpContent[contentLen];
		strcpy(tmpContent, content);
		//establish used block list
		int* freeBlockList = findMultiFreeBlocks(neededBlock);
		arr* usedBlockList = malloc(sizeof(arr));
		int i;
		for (i = 0; i < 400; i++) {
			usedBlockList->nums[i] = 0;
		}
		int freeBlockListSize = getBlockSize(freeBlockList);
		//put original block into block list
		usedBlockList->nums[0] = infoMap->location[0];
		for (i = 1; i < freeBlockListSize + 1; i++) {
			usedBlockList->nums[i] = freeBlockList[i - 1];
			eraseFile(freeBlockList[i - 1]);
		}
//		printf("in append\n");
//		for(i=0;i<100;i++){
//			printf("arr is:%d\n",usedBlockList->nums[i]);
//		}
//		printf("end append\n");
		//update index block to inode
		infoMap->location[0] = indexBlock;
		//store used block list
		writeArrayToBlock(indexBlock, usedBlockList);
		int lastWrite = infoMap->lastWriteLocation;
		//start writing file
		for (i = 0; i < freeBlockListSize; i++) {
			//last block
			if (i == freeBlockListSize - 1) {
				char* tmpFileName = getFileName(freeBlockList[i]);
				FILE *file = fopen(tmpFileName, "a+");
				fwrite(tmpContent, sizeof(char), strlen(tmpContent), file);
				fclose(file);
				lastWrite = freeBlockList[i];
				free(tmpFileName);
			} else {
				//get first BLOCKSIZE
				char *tmpContent = substring(0, BLOCKSIZE, tmpContent);
				char* tmpFileName = getFileName(freeBlockList[i]);
				FILE *file = fopen(tmpFileName, "a+");
				fwrite(tmpContent, sizeof(char), BLOCKSIZE, file);
				fclose(file);
				//generate BLOCKSIZE-end string
				char *tmpRemain = substring(BLOCKSIZE,
						strlen(tmpContent) - BLOCKSIZE, tmpContent);
				strcpy(tmpContent, tmpRemain);
				free(tmpRemain);
				free(tmpFileName);
				free(tmpContent);
			}
		}
		free(freeBlockList);
		free(usedBlockList);
//		printf("finish 3rd last write is:%d\n",infoMap->lastWriteLocation);
		return lastWrite;
	}
	return 0;
}
//WRITE
int fs_write(const char* path, const char* buf, size_t count, off_t offset,
		struct fuse_file_info *fuseFileInfo) {

	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		free(aPath);
		free(truePath);
		return -ENOENT;
	}
	if (strlen(buf) == 0) {
		free(aPath);
		free(truePath);
		return count;
	}
	int iNode = getInPathToInodeMap(truePath);
	//not open
	if (findInOpenedFileMap(iNode) == -1) {
		free(aPath);
		free(truePath);
		return -ENOENT;
	}
	//read only
	if ((O_RDONLY & getInFileInfoMap(iNode)->flags) != 0) {
		free(aPath);
		free(truePath);
		return -EACCES;
	}
//	printf("inode is:%d\n", iNode);
	inodeBlock *infoMap = getInodeInfoData(iNode);
	//not a regular file
	if (!S_ISREG(infoMap->mode)) {
		free(aPath);
		free(truePath);
		free(infoMap);
		return -EISNAM;
	}
	if (infoMap->size + count > MAXSIZE) {
		free(aPath);
		free(truePath);
		free(infoMap);
		return -EFBIG;
	}
	int bufLen = (int) strlen(buf);
	char content[bufLen];
	strcpy(content, buf);
//	printf("before write count is:%d\n", (int) count);
//	printf("before write buf length is:%d\n", (int) strlen(content));
	if (count < strlen(content)) {
		content[count] = '\0';
	}
	int lastWrite = infoMap->lastWriteLocation;
//	int position = getInFileInfoMap(iNode)->position;
//	position += offset; //we don't care at this time
//	printf("count is:%d\n", (int) count);
//	int blockNumber = getInFileInfoMap(iNode)->blocks;
	if (count == 4096) {
		content[4095] = '\0';
		lastWrite = appendToFile(content, infoMap);
	} else if ((O_APPEND & fuseFileInfo->flags) && infoMap->valid == -2) {
		lastWrite = appendToFile(content, infoMap);
	} else {
		if (count <= BLOCKSIZE) {
			if (infoMap->valid != -2) {
				int freeBlock = findFreeBlock();
				eraseFile(freeBlock);
				infoMap->valid = -2;
				infoMap->location[0] = freeBlock;
				infoMap->lastWriteLocation = freeBlock;
				lastWrite = appendToFile(content, infoMap);
			} else {
				lastWrite = appendToFile(content, infoMap);
			}
		} else {
			lastWrite = appendToFile(content, infoMap);
		}
	}
//	printf("in write last write is:%d\n",infoMap->lastWriteLocation);
	infoMap->lastWriteLocation = lastWrite;
//	printf("return last write is:%d\n",infoMap->lastWriteLocation);
	infoMap->size += count;
	writeInodeToBlock(iNode, infoMap);
//	printf("after write count is:%d\n", (int) count);
//	printf("after write size is:%d\n", infoMap->size);
	free(aPath);
	free(truePath);
	free(infoMap);
	return strlen(content);
}
//READ
int fs_read(const char* path, char* result, size_t count, off_t offset,
		struct fuse_file_info *fuseFile) {
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	int iNode = getInPathToInodeMap(truePath);
	//not open
	if (findInOpenedFileMap(iNode) == -1) {
		return -ENOENT;
	}
	//not a regular file
//	if (!S_ISREG(getInodeInfoData(iNode)->mode)) {
//		return -ENOENT;
//	}
//	int startPosition = 0;
//	startPosition += offset;

	inodeBlock *infoMap = getInodeInfoData(iNode);
	if (infoMap->lastWriteLocation == 0) {
		printf(
				"Nothing to read!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		free(truePath);
		free(aPath);
		free(infoMap);
		return -ECANCELED;
	}
//	int startBlockNumber = infoMap->location[0];
	char* strData = malloc(sizeof(char) * infoMap->size);
//	printf("size is: %d\n", infoMap->size);
	//file is in one block only
	if (infoMap->indirect == 0) {
		char* fileName = getFileName(infoMap->location[0]);
		insertIntoFileInfoMap(getInFileInfoMap(iNode)->inode,
				getInFileInfoMap(iNode)->position + strlen(strData),
				getInFileInfoMap(iNode)->blocks,
				getInFileInfoMap(iNode)->flags);
		FILE *fp = fopen(fileName, "r");
		fread(result, sizeof(char), BLOCKSIZE, fp);
		fclose(fp);
		free(fileName);
		free(strData);
		free(truePath);
		free(aPath);
		free(infoMap);
		return count;
	} else {
		arr* tmpListHolder = getArrayData(infoMap->location[0]);
		int tmpListSize = getBlockSize(tmpListHolder->nums);
		int blockList[tmpListSize];
		int i;
		for (i = 0; i < tmpListSize; i++) {
			blockList[i] = tmpListHolder->nums[i];
		}
		for (i = 0; i < tmpListSize; i++) {
			char *fileName = getFileName(blockList[i]);
			if (i == 0) {
				FILE *fp = fopen(fileName, "r");
				fread(result, sizeof(char), BLOCKSIZE, fp);
				fclose(fp);
			} else {
				FILE *fp = fopen(fileName, "r");
				char readRes[4096];
				fread(readRes, sizeof(char), BLOCKSIZE, fp);
				fclose(fp);
				strcat(result, readRes);
			}
			free(fileName);
		}
		free(strData);
		free(truePath);
		free(aPath);
		free(infoMap);
		free(tmpListHolder);
		return count;
	}
}
//OPEN
int fs_open(const char* path, struct fuse_file_info *fuseFileInfo) {
	//not a valid path
	if (strcmp(path, "") == 0) {
		return -EFAULT;
	}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//file doesn't exist, we create one here
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	//these kinds of flag should not be in here
	if ((O_CREAT & fuseFileInfo->flags) && (O_EXCL & fuseFileInfo->flags)) {
		return -EACCES;
	}
	int iNode = getInPathToInodeMap(truePath);
	//it's a regular file
//	if (S_ISREG(getInodeInfoData(iNode)->mode)) {
	if (findInOpenedFileMap(iNode) == -1) {
		inodeBlock *tmpInfo = getInodeInfoData(iNode);
		char* tmpName = getFileName(iNode);
		insertIntoOpenedFileMap(iNode, tmpName);
		free(tmpInfo);
		free(tmpName);
//			printf("valid is:%d\n",openedFileMapValid);
	}
	insertIntoFileInfoMap(iNode, 0, iNode, 0);
	insertIntoFileInfoMap(iNode, getInFileInfoMap(iNode)->position,
			getInFileInfoMap(iNode)->blocks, 0);
//	int i;
//	for(i=0;i<openedFileMapValid;i++){
//		printf("%d\n",openedFileMapiNodes[i]);
//		printf("%s\n",openedFileMapTruePath[i]);
//	}
	return 0;
}
/***************************************************************************************/
//INIT
void* fs_init(struct fuse_conn_info *conn) {
	removeAllFile();
	insertIntoPathToInodeMap("/", ROOT_DIRECTORY_INODE);
	int i;
	for (i = 0; i < TOTALFILE; i++) {
		createFile(i);
	}
	arr *tmpBlockList = malloc(sizeof(arr));
	for (i = 0; i < 400; i++) {
		tmpBlockList->nums[i] = 0;
	}
	int *tmpNumList = generateNumList(27, 400);
	for (i = 0; i < 400 - 27; i++) {
		tmpBlockList->nums[i] = tmpNumList[i];
	}
	writeArrayToBlock(1, tmpBlockList);
	free(tmpBlockList);
	for (i = 2; i < 26; i++) {
		arr *currList = malloc(sizeof(arr));
		int j;
		for (j = 0; j < 400; j++) {
			currList->nums[i] = 0;
		}
		int* tmp = generateNumList(400 * (i - 1), 400 * i);
		for (j = 0; j < 400; j++) {
			currList->nums[i] = tmp[i];
		}
		writeArrayToBlock(i, currList);
		free(tmp);
		free(currList);
	}
	superBlock *SUPER_BLOCK = malloc(sizeof(superBlock));
	SUPER_BLOCK->creationTime = 1376483073;
	SUPER_BLOCK->dev_id = 20;
	SUPER_BLOCK->mounted = 1;
	SUPER_BLOCK->freeStat = 1;
	SUPER_BLOCK->freeEnd = 25;
	SUPER_BLOCK->root = 26;
	SUPER_BLOCK->maxBlocks = TOTALFILE;
	writeSuperBlocktoBlock(SUPER_BLOCK);
	free(SUPER_BLOCK);
	inodeBlock *root = malloc(sizeof(inodeBlock));
	root->atime = 1376483073;
	root->ctime = 1376483073;
	root->mtime = 1376483073;
	root->size = 0;
	root->uid = 1;
	root->gid = 1;
	root->mode = 16877;
	root->linkcount = 2;
	strcpy(root->content[0], "d.");
	strcpy(root->content[1], "d..");
	root->location[0] = 26;
	root->location[1] = 26;
	root->valid = 2;
	writeInodeToBlock(26, root);
	free(root);
	return NULL;
}
//LINK
int fs_link(const char* oldPath, const char* newPath) {
	char* aOldPath = malloc(sizeof(char) * 255);
	strcpy(aOldPath, oldPath);
	char* trueOldPath = getAbsolutePath(aOldPath);
	//old path doesn't exist
	if (findInPathToInodeMap(trueOldPath) == -1) {
		return -ENOENT;
	}
	int oldInode = getInPathToInodeMap(trueOldPath);
	//old path is a dir
	if ((getInodeInfoData(oldInode)->mode == 16877)) {
		return -EISDIR;
	}
	//not a valid path
	if (strcmp(newPath, "") == 0) {
		return -EFAULT;
	}
	char* aNewPath = malloc(sizeof(char) * 255);
	strcpy(aNewPath, newPath);
	char* trueNewPath = getAbsolutePath(aNewPath);
	//new path already exists
	if (findInPathToInodeMap(trueNewPath) != -1) {
		return -EEXIST;
	}
	char* trueNewParentPath = getAbsParentPath(aNewPath);
	//new path's parent doesn't exist
	if (findInPathToInodeMap(trueNewParentPath) == -1) {
		return -ENOENT;
	}
	int newParentInode = getInPathToInodeMap(trueNewParentPath);
	//new path's parent is not a dir
//	if (!S_ISDIR(getInodeInfoData(oldInode)->mode)) {
//		return -ENOENT;
//	}
//	int i;
	char *tmpTrue = malloc(sizeof(trueNewPath));
	strcpy(tmpTrue, trueNewPath);
	char *newDirName = getLastOneinString(splitString(tmpTrue, '/'));
//	printf("new dir name is:%s\n",newDirName);
	inodeBlock *oldFileInode = getInodeInfoData(oldInode);
	oldFileInode->linkcount = oldFileInode->linkcount + 1;
	writeInodeToBlock(oldInode, oldFileInode);
	inodeBlock *parentInodeInfo = getInodeInfoData(newParentInode);
	char *tmp = getCat("f", newDirName);
//	for (i = 0; i < parentInodeInfo->valid; i++) {
//		printf("prev path is:%s\n", parentInodeInfo->content[i]);
//		printf("prev inode is:%d\n", parentInodeInfo->location[i]);
//	}
	insertIntoInodeBlock(parentInodeInfo, tmp, oldInode);
//	for (i = 0; i < parentInodeInfo->valid; i++) {
//		printf("curr path is:%s\n", parentInodeInfo->content[i]);
//		printf("curr inode is:%d\n", parentInodeInfo->location[i]);
//	}
	parentInodeInfo->linkcount = parentInodeInfo->linkcount + 1;
	writeInodeToBlock(newParentInode, parentInodeInfo);
	insertIntoPathToInodeMap(trueNewPath, oldInode);
	free(trueOldPath);
	free(aOldPath);
	free(aNewPath);
	free(trueNewPath);
	free(trueNewParentPath);
	free(newDirName);
	free(oldFileInode);
	free(parentInodeInfo);
	free(tmp);
	return 0;
}
//STATFS
int fs_statfs(const char* path, struct statvfs* aStatvfs) {
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	aStatvfs->f_bsize = BLOCKSIZE;
	aStatvfs->f_blocks = TOTALFILE;
	int freeBlocks = 0;
	int i;
	for (i = 1; i < 26; i++) {
		arr *tmp = getArrayData(i);
		freeBlocks += getBlockSize(tmp->nums);
		free(tmp);
	}
	aStatvfs->f_bfree = freeBlocks;
	aStatvfs->f_bavail = freeBlocks;
	aStatvfs->f_files = pathToInodeMapValid + 1; //plus root inode
	aStatvfs->f_namemax = 255;
	aStatvfs->f_ffree = 0;
	free(aPath);
	free(truePath);
	return 0;
}
//CREATE
int fs_create(const char* path, mode_t mode,
		struct fuse_file_info* fuseFileInfo) {
	//not a valid path
	if (strcmp(path, "") == 0) {
		return -EFAULT;
	}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	if (findInPathToInodeMap(truePath) == -1) {
		//cannot create
//		if (!(O_CREAT & fuseFileInfo->flags)) {
//			return -ENOENT;
//		}
		char* trueParentPath = getAbsParentPath(aPath);
		//parent doesn't exist
		if (findInPathToInodeMap(trueParentPath) == -1) {
			return -ENOENT;
		}
		int parentInode = getInPathToInodeMap(trueParentPath);
		//parent isn't a dir
		char *tmpTrue = malloc(sizeof(truePath));
		strcpy(tmpTrue, truePath);
		char* fileName = getLastOneinString(splitString(tmpTrue, '/'));
		int iNodeBlock = findFreeBlock();
//		int locationBlock = findFreeBlock();
		inodeBlock *newInodeInfo = malloc(sizeof(inodeBlock));
		newInodeInfo->size = 0;
		newInodeInfo->uid = 1000;
		newInodeInfo->gid = 1000;
		newInodeInfo->mode = S_IFREG | mode;
		newInodeInfo->atime = 1323630836;
		newInodeInfo->ctime = 1323630836;
		newInodeInfo->mtime = 1323630836;
		newInodeInfo->linkcount = 1;
		newInodeInfo->indirect = 0;
		newInodeInfo->lastWriteLocation = 0;
//		newInodeInfo->location[0] = locationBlock;
		newInodeInfo->valid = -1;
		writeInodeToBlock(iNodeBlock, newInodeInfo);
		inodeBlock *pInode = getInodeInfoData(parentInode);
//		printf("filenname:%s\n", fileName);
		char *catName = getCat("f", fileName);
		insertIntoInodeBlock(pInode, catName, iNodeBlock);
		pInode->linkcount = pInode->linkcount + 1;
		writeInodeToBlock(parentInode, pInode);
		insertIntoPathToInodeMap(truePath, iNodeBlock);
		insertIntoFileInfoMap(iNodeBlock, 0, -1, 0); //insertIntoFileInfoMap(iNodeBlock, 0, locationBlock, 0);
		free(trueParentPath);
		free(tmpTrue);
		free(fileName);
		free(newInodeInfo);
		free(pInode);
		free(catName);
	} else {
		free(aPath);
		free(truePath);
		return -EEXIST;
	}
//test use
//	int j;
//	for(j=0;j<pathToInodeMapValid;j++){
//		printf("true path:%s\n",pathToInodeMapTruePath[j]);
//		printf("inode number:%d\n",pathToInodeMapiNodes[j]);
//	}
	free(aPath);
	free(truePath);
	return 0;
}
//MKDIR
int fs_mkdir(const char* path, mode_t mode) {
	//not a valid path
	if (strcmp(path, "") == 0) {
		return -EFAULT;
	}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//already exist
	if (findInPathToInodeMap(truePath) == 0) {
		return -EEXIST;
	}
	char* trueParentPath = getAbsParentPath(aPath);
	//parent doesn't exist
	if (findInPathToInodeMap(trueParentPath) == -1) {
		return -ENOENT;
	}

	int parentInode = getInPathToInodeMap(trueParentPath);
	inodeBlock *parentInfo = getInodeInfoData(parentInode);
	//parent isn't a dir
	if (!S_ISDIR(parentInfo->mode)) {
		return -ENOENT;
	}
	char *tmpTrue = malloc(sizeof(truePath));
	strcpy(tmpTrue, truePath);
	char* dirName = getLastOneinString(splitString(tmpTrue, '/'));
	int dirBlock = findFreeBlock();
	inodeBlock *newInodeEntry = malloc(sizeof(inodeBlock));
	newInodeEntry->size = 0;
	newInodeEntry->uid = 1000;
	newInodeEntry->gid = 1000;
	newInodeEntry->mode = 16877;
	newInodeEntry->atime = 1323630836;
	newInodeEntry->ctime = 1323630836;
	newInodeEntry->mtime = 1323630836;
	newInodeEntry->linkcount = 1;
	strcpy(newInodeEntry->content[0], "d.");
	strcpy(newInodeEntry->content[1], "d..");
	newInodeEntry->location[0] = dirBlock;
	newInodeEntry->location[1] = parentInode;
	newInodeEntry->valid = 2;
	writeInodeToBlock(dirBlock, newInodeEntry);
	parentInfo->location[parentInfo->valid] = dirBlock;
	char *tmp = getCat("d", dirName);
	strcpy(parentInfo->content[parentInfo->valid], tmp);
	parentInfo->valid = parentInfo->valid + 1;
	parentInfo->linkcount = parentInfo->linkcount + 1;
	writeInodeToBlock(parentInode, parentInfo);
	insertIntoPathToInodeMap(truePath, dirBlock);
	free(aPath);
	free(truePath);
	free(dirName);
	free(tmpTrue);
	free(newInodeEntry);
	free(tmp);
	free(parentInfo);
	return 0;
}
//READ DIR
int fs_readdir(const char* path, void* buf, fuse_fill_dir_t fuseFiller,
		off_t offset, struct fuse_file_info *fuseFileInfo) {
//test use
//		int j;
//		for(j=0;j<pathToInodeMapValid;j++){
//			printf("true path:%s\n",pathToInodeMapTruePath[j]);
//			printf("inode number:%d\n",pathToInodeMapiNodes[j]);
//		}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
//	printf("get attr true path:%s\n",truePath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	int iNode = getInPathToInodeMap(truePath);
//	printf("get inode number%d\n",iNode);
	inodeBlock *infoMap = getInodeInfoData(iNode);
//	printf("info map: %d\n",infoMap->valid);
	int i;
	for (i = 2; i < infoMap->valid; i++) {
		char* tmpInfo = malloc(sizeof(char) * 255);
		strcpy(tmpInfo, infoMap->content[i]);
//		printf("tmpinfo:%s\n",tmpInfo);
		fuseFiller(buf, substring(1, strlen(tmpInfo) - 1, tmpInfo), NULL, 0);
		free(tmpInfo);
	}
	free(aPath);
	free(truePath);
	free(infoMap);
	return 0;
}
//GET ATTR
int fs_getattr(const char* path, struct stat *res) {
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	int iNode = getInPathToInodeMap(truePath);
	inodeBlock *info = getInodeInfoData(iNode);
	res->st_mode = info->mode;
	res->st_nlink = info->linkcount;
	res->st_uid = info->uid;
	res->st_gid = info->gid;
	res->st_size = info->size;
	res->st_atime = info->atime;
	res->st_mtime = info->mtime;
	res->st_ctime = info->ctime;
	res->st_blksize = BLOCKSIZE;
	res->st_blocks = info->indirect;
//test use
//	int i;
//	printf("validvalidvalidvalidvalid");
//	printf("%d\n",pathToInodeMapValid);
//	for(i=0;i<pathToInodeMapValid;i++){
//		printf("startstartstartstart:\n");
//		printf(pathToInodeMapTruePath[i]);
//		printf("\n");
//	}
	free(aPath);
	free(truePath);
	free(info);
	return 0;
}
//OPEN DIR
int fs_opendir(const char* path, struct fuse_file_info *fuseFileInfo) {
	if (strcmp(path, "") == 0) {
		return -EFAULT;
	}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	if (findInPathToInodeMap(truePath) == -1) {
		//cannot create a dir
//		if (!(O_CREAT & fuseFileInfo->flags)) {
//			return -ENOENT;
//		}
		return -ENOENT;
	}
	int iNode = findInPathToInodeMap(truePath);
	//not a dir
//	if (!S_ISDIR(getInodeInfoData(iNode)->mode)) {
//		return -ENOENT;
//	}
	insertIntoOpenedFileMap(iNode, getFileName(iNode));
	free(aPath);
	free(truePath);
	return 0;
}
//UNLINK
int fs_unlink(const char* path) {
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	int currInode = getInPathToInodeMap(truePath);
	//it's a dir
//	if (S_ISDIR(getInodeInfoData(currInode)->mode)) {
//		return -ENOENT;
//	}
	char* trueParentPath = getAbsParentPath(aPath);
	int parentInode = getInPathToInodeMap(trueParentPath);
	char* tmpTrue = malloc(sizeof(truePath));
	strcpy(tmpTrue, truePath);
	char *fileName = getLastOneinString(splitString(tmpTrue, '/'));
	inodeBlock *parentInodeInfo = getInodeInfoData(parentInode);
	char* catName = getCat("f", fileName);
//test use
//	int j;
//	printf("parent path is:%s\n",trueParentPath);
//	for(j=0;j<pathToInodeMapValid;j++){
//		printf("inode is %d\n",pathToInodeMapiNodes[j]);
//		printf("path is %s\n",pathToInodeMapTruePath[j]);
//	}
//	printf("parent is:%d\n",parentInode);
//	printf("valid:%d\n",parentInodeInfo->valid);
//	for (j = 0; j < parentInodeInfo->valid; j++) {
//		printf("%s\n", parentInodeInfo->content[j]);
//		printf("%d\n", parentInodeInfo->location[j]);
//	}
	deleteInInodeBlock(parentInodeInfo, catName);
	parentInodeInfo->linkcount = parentInodeInfo->linkcount - 1;
//test use
//	printf("valid:%d\n",parentInodeInfo->valid);
//	for (j = 0; j < parentInodeInfo->valid; j++) {
//		printf("%s\n", parentInodeInfo->content[j]);
//		printf("%d\n", parentInodeInfo->location[j]);
//	}
	writeInodeToBlock(parentInode, parentInodeInfo);
	deleteInPathToInodeMap(truePath);
	inodeBlock *fileInodeInfo = getInodeInfoData(currInode);
	fileInodeInfo->linkcount = fileInodeInfo->linkcount - 1;
	if (fileInodeInfo->linkcount > 0) {
		writeInodeToBlock(currInode, fileInodeInfo);
	} else {
		addOneToFreeBlock(currInode);
		if (fileInodeInfo->indirect == 0) {
			if (fileInodeInfo->valid == -2) {
				addOneToFreeBlock(fileInodeInfo->location[0]);
			}
		} else {
			if (fileInodeInfo->valid == -2) {
				int location = fileInodeInfo->location[0];
				int* blockList = getArrayData(location)->nums;
				addOneToFreeBlock(location);
				addMultipleToFreeBlock(blockList);
				free(blockList);
			}

		}
	}
	free(aPath);
	free(truePath);
	free(trueParentPath);
	free(fileName);
	free(parentInodeInfo);
	free(fileInodeInfo);
	return 0;
}
//RENAME
int fs_rename(const char* oldPath, const char* newPath) {
	char* aOldPath = malloc(sizeof(char) * 255);
	strcpy(aOldPath, oldPath);
	char* aNewPath = malloc(sizeof(char) * 255);
	strcpy(aNewPath, newPath);
	char* trueOldPath = getAbsolutePath(aOldPath);
	char* trueNewPath = getAbsolutePath(aNewPath);
	//doesn't exist
	if (findInPathToInodeMap(trueOldPath) == -1) {
		return -ENOENT;
	}
	if (strcmp(getAbsParentPath(trueNewPath), getAbsParentPath(trueOldPath))
			!= 0) {
		printf("This operation is not supported!!!!!!!!!!!!\n");
		return -ENOTSUP;
	}
	if (findInPathToInodeMap(trueNewPath) == 0) {
		return -EEXIST;
	}
	//not a valid name
	if (strcmp(trueNewPath, "") == 0) {
		return -EFAULT;
	}
	char* trueOldParentPath = getAbsParentPath(trueOldPath);
	int parentInode = getInPathToInodeMap(trueOldParentPath);
	inodeBlock *parentInodeBlock = getInodeInfoData(parentInode);
	int iNode = getInPathToInodeMap(trueOldPath);
	inodeBlock *currInodeBlock = getInodeInfoData(iNode);
	char* tmpTrueNew = malloc(sizeof(trueNewPath));
	strcpy(tmpTrueNew, trueNewPath);
	char* newFileName = getLastOneinString(splitString(tmpTrueNew, '/'));
	if (currInodeBlock->valid < 0) {
		insertIntoInodeBlock(parentInodeBlock, getCat("f", newFileName), iNode);
	} else {
		printf(
				"This operation is not supported, only support rename between regular files!!!!!!!!!\n");
		return -ENOTSUP;
	}
	insertIntoPathToInodeMap(trueNewPath, iNode);
	char *tmpTrueOld = malloc(sizeof(trueOldPath));
	strcpy(tmpTrueOld, trueOldPath);
	char* oldFileName = getLastOneinString(splitString(tmpTrueOld, '/'));
	if (currInodeBlock->valid < 0) {
		deleteInInodeBlock(parentInodeBlock, getCat("f", oldFileName));
	} else {
		printf(
				"This operation is not supported, only support rename between regular files!!!!!!!!!\n");
		return -ENOTSUP;
	}
	deleteInPathToInodeMap(trueOldPath);
	writeInodeToBlock(parentInode, parentInodeBlock);
	free(aOldPath);
	free(aNewPath);
	free(trueOldPath);
	free(trueNewPath);
	free(trueOldParentPath);
	free(parentInodeBlock);
	free(oldFileName);
	free(newFileName);
	return 0;
}
//RELEASE
int fs_release(const char* path, struct fuse_file_info *fuseFile) {
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	int iNode = getInPathToInodeMap(truePath);
	//its a dir
//	if (S_ISDIR(getInodeInfoData(iNode)->mode)) {
//		return -ENOENT;
//	}
	deleteInOpenedFileMap(iNode);
	free(aPath);
	free(truePath);
	return 0;
}
//RELEASE DIR
int fs_releasedir(const char* path, struct fuse_file_info *fuseFile) {
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	int iNode = getInPathToInodeMap(truePath);
	//its not a dir
//	if (!S_ISDIR(getInodeInfoData(iNode)->mode)) {
//		return -ENOENT;
//	}
	deleteInOpenedFileMap(iNode);
	free(aPath);
	free(truePath);
	return 0;
}
//READ LINK
int fs_readlink(const char* path, char *res, size_t count) {
	if (strcmp(path, "") == 0) {
		return -EFAULT;
	}
	char* aPath = malloc(sizeof(char) * 255);
	strcpy(aPath, path);
	char* truePath = getAbsolutePath(aPath);
	//doesn't exist
	if (findInPathToInodeMap(truePath) == -1) {
		return -ENOENT;
	}
	struct fuse_file_info *fuseFileInfo = { O_RDONLY };
	fs_read(path, res, count, 0, fuseFileInfo);
	free(aPath);
	free(truePath);
	return 0;
}
//DESTROY
void fs_destroy() {
	removeAllFile();
	exit(0);
}
//UTIMENS
int fs_utimens(const char* path, const struct timespec ts[2]) {
	return 0;
}
//CHMOD
int fs_chmod(const char *path, mode_t mode) {
	return 0;
}
//CHOWN
int fs_chown(const char *path, uid_t uid, gid_t gid) {
	return 0;
}
static struct fuse_operations ops = { .chown = fs_chown, .chmod = fs_chmod,
		.utimens = fs_utimens, .create = fs_create, .readlink = fs_readlink,
		.read = fs_read, .destroy = fs_destroy, .release = fs_release, .write =
				fs_write, .rename = fs_rename, .link = fs_link, .statfs =
				fs_statfs, .unlink = fs_unlink, .mkdir = fs_mkdir, .releasedir =
				fs_releasedir, .open = fs_open, .opendir = fs_opendir,
		.readdir = fs_readdir, .getattr = fs_getattr, .init = fs_init,
		.truncate = fs_truncate, };
int main(int argc, char *argv[]) {
	printf("mounting file system...\n");
	printf("Make sure you are under super user mode\n");
	printf("mv is only supported between files under the same directory\n");
	printf(
			"you can touch one file twice, but the second touch will do nothing\n");
	printf(
			"if you want to change max block size, # of fusedata.x created and max file size, you can change it at the top of this program\n");
	return fuse_main(argc, argv, &ops, NULL);
}
