# -*- coding: utf-8 -*-
__author__ = 'Guoqing Xiong'
import time

FILE_PREFIX = "/fusedata/fusedata."
CURR_TIME = time.time()
BLOCK_SIZE = 4096

# deserialize string to an appropriate object
def deserialize(str):
    original = str
    str = str.replace("{", "")
    str = str.replace("}", "")
    str = str.replace(" ", "")
    if 'creationTime' in str:
        infoStrs = str.split(",")
        res = {}
        for i in infoStrs:
            if len(i) > 0:
                tempInfo = i.split(":")
                res[tempInfo[0]] = int(tempInfo[1])
        return res
    elif 'filename_to_inode_dict' in str:
        infoStrs = str.split(",")
        res = {}
        filename_to_inode_dict = {}
        for i in infoStrs:
            if len(i) > 0 and 'filename_to_inode_dict' not in i:
                tempInfo = i.split(":")
                res[tempInfo[0]] = int(tempInfo[1])
                continue
            if 'filename_to_inode_dict' in i:
                break
        tempStr = original.split("{")[2]
        tempStr = tempStr.replace("}", "")
        tempStr = tempStr.replace(" ", "")
        tempDictHolder = tempStr.split(",")
        for i in tempDictHolder:
            if len(i) > 0:
                tempInfo = i.split(":")
                filename_to_inode_dict[tempInfo[0] + ":" + tempInfo[1]] = int(tempInfo[2])
        res['filename_to_inode_dict'] = filename_to_inode_dict
        return res
    elif 'location' in str:
        infoStrs = str.split(",")
        res = {}
        for i in infoStrs:
            if len(i) > 0:
                tempInfo = i.split(":")
                if 'location' in tempInfo[1]:
                    res[tempInfo[0]] = int(tempInfo[1][:tempInfo[1].find('location')])
                    res['location'] = int(tempInfo[2])
                    continue
                res[tempInfo[0]] = int(tempInfo[1])
        return res
    else:
        numStrs = str.split(',')
        res = []
        for i in numStrs:
            if len(i) > 0:
                res.append(int(i))
        res.sort()
        return res


#get data from block
def getData(blockNumber):
    f = open(FILE_PREFIX + str(blockNumber))
    content = f.read()
    f.close()
    return deserialize(content)


#write serialized obj to block
def writeToBlock(obj, blocknumber):
    if type(obj) == list:
        res = ''
        for i in obj:
            res += str(i) + ","
        f = open(FILE_PREFIX + str(blocknumber), 'w')
        f.write(res[:-1])
        f.close()
    else:
        if 'filename_to_inode_dict' in obj:
            resOne = '{size:%d, uid:%d, gid:%d, mode:%d, atime:%d, ctime:%d, mtime:%d, linkcount:%d, filename_to_inode_dict: ' % (
                obj['size'], obj['uid'], obj['gid'], obj['mode'], obj['atime'], obj['ctime'], obj['mtime'],
                obj['linkcount'])
            resTwo = '{'
            for key in obj['filename_to_inode_dict']:
                resTwo += key + ":" + str(obj['filename_to_inode_dict'][key]) + ","
            resTwo = resTwo[:-1]
            resTwo += "}}"
            res = resOne + resTwo
            f = open(FILE_PREFIX + str(blocknumber), 'w')
            f.write(res)
            f.close()
        elif "creationTime" in obj:
            res = '{creationTime: %d, mounted: %d, devId:%d, freeStart:%d, freeEnd:%d, root:%d, maxBlocks:%d}' % (
                obj['creationTime'], obj['mounted'], obj['devId'], obj['freeStart'], obj['freeEnd'], obj['root'],
                obj['maxBlocks'])
            f = open(FILE_PREFIX + str(blocknumber), 'w')
            f.write(res)
            f.close()
        else:
            res = '{size:%d, uid:%d, gid:%d, mode:%d, linkcount:%d, atime:%d, ctime:%d, mtime:%d, indirect:%d, location:%d}' % (
                obj['size'], obj['uid'], obj['gid'], obj['mode'], obj['linkcount'], obj['atime'], obj['ctime'],
                obj['mtime'], obj['indirect'], obj['location'])
            f = open(FILE_PREFIX + str(blocknumber), 'w')
            f.write(res)
            f.close()


#add this block to free block list
def addTofreeblock(blocknumber):
    f = open(FILE_PREFIX + str(blocknumber), 'w')
    f.write('0' * 4096)
    f.close()
    data = getData(blocknumber / 400 + 1)
    if blocknumber not in data:
        data.append(blocknumber)
    data.sort()
    writeToBlock(data, blocknumber / 400 + 1)


#delete a block from free block list
def deleteFromFreeBlock(blocknumber):
    data = getData(blocknumber / 400 + 1)
    if blocknumber in data:
        data.remove(blocknumber)
    data.sort()
    writeToBlock(data, blocknumber / 400 + 1)


def checkDevID():
    superBlock = getData(0)
    if superBlock['devId'] != 20:
        print "Device ID is wrong"
        print "End checking"
        exit()


def checkTime():
    def timeChecker(blockNumber):
        currBlock = getData(blockNumber)
        if currBlock['atime'] > CURR_TIME or currBlock['ctime'] > CURR_TIME or currBlock['mtime'] > CURR_TIME:
            print "Block %d's time is wrong" % (blockNumber)
            currBlock['atime'] = min(CURR_TIME, currBlock['atime'])
            currBlock['ctime'] = min(CURR_TIME, currBlock['ctime'])
            currBlock['mtime'] = min(CURR_TIME, currBlock['mtime'])
            writeToBlock(currBlock, blockNumber)
        if 'filename_to_inode_dict' not in currBlock:
            return
        else:
            for child in currBlock['filename_to_inode_dict']:
                if child == 'd:.' or child == 'd:..':
                    continue
                timeChecker(currBlock['filename_to_inode_dict'][child])

    superBlock = getData(0)
    if superBlock['creationTime'] > CURR_TIME:
        print "super block creation time is wrong"
        superBlock['creationTime'] = CURR_TIME
        writeToBlock(superBlock, 0)
    timeChecker(26)


def checkDir(parentNumber=26, blockNumber=26):
    currBlock = getData(blockNumber)
    if 'filename_to_inode_dict' not in currBlock:
        if blockNumber == 26:
            print "Root's child directory doesn't exist"
            currBlock['filename_to_inode_dict'] = {}
            currBlock['filename_to_inode_dict']["d:."] = 26
            currBlock['filename_to_inode_dict']["d:.."] = 26
    if len(currBlock['filename_to_inode_dict']) != currBlock['linkcount']:
        print "Block %d's link count is wrong" % blockNumber
        currBlock['linkcount'] = len(currBlock['filename_to_inode_dict'])
    if "d:." not in currBlock['filename_to_inode_dict'] or 'd:..' not in currBlock['filename_to_inode_dict']:
        print "Block %d should have d. and d.." % blockNumber
        currBlock['filename_to_inode_dict']['d:.'] = blockNumber
        currBlock['filename_to_inode_dict']['d:..'] = parentNumber
    if 'd:.' in currBlock['filename_to_inode_dict'] and currBlock['filename_to_inode_dict']['d:.'] != blockNumber:
        print "Block %d's . number is wrong" % blockNumber
        currBlock['filename_to_inode_dict']['d:.'] = blockNumber
    if 'd:..' in currBlock['filename_to_inode_dict'] and currBlock['filename_to_inode_dict']['d:..'] != parentNumber:
        print "Block %d's .. number is wrong" % blockNumber
        currBlock['filename_to_inode_dict']['d:..'] = parentNumber
    writeToBlock(currBlock, blockNumber)
    for child in currBlock['filename_to_inode_dict']:
        if child == "d:." or child == "d:.." or child[0] != 'd':
            continue
        checkDir(blockNumber, currBlock['filename_to_inode_dict'][child])


def checkFreeBlockList():
    def freeBlockListChecker(blockNumber=26):
        if blockNumber in freeBlockList:
            print "Block %d should not be in free block list" % blockNumber
            deleteFromFreeBlock(blockNumber)
        allBlockList.remove(blockNumber)
        currBlock=getData(blockNumber)
        if "filename_to_inode_dict" in currBlock:
            for child in currBlock['filename_to_inode_dict']:
                if child=='d:.' or child=='d:..':
                    continue
                freeBlockListChecker(currBlock['filename_to_inode_dict'][child])
        else:
            try:
                if currBlock['location'] in freeBlockList:
                    print "Block %d should not be in free block list" % currBlock['location']
                    deleteFromFreeBlock(currBlock['location'])
                allBlockList.remove(currBlock['location'])
                locations=getData(currBlock['location'])
                for i in locations:
                    allBlockList.remove(i)
                    if i in freeBlockList:
                        print "Block %d should not be in free block list" %i
                        deleteFromFreeBlock(i)
            except:
                return
    freeBlockList = set()
    allBlockList=set()
    for i in range(26,10000):
        allBlockList.add(i)
    for i in range(1, 26):
        currList = getData(i)
        for j in currList:
            freeBlockList.add(j)
    freeBlockListChecker()
    for i in range(0,26):
        if i in freeBlockList:
            print "Block %d should not be in free block list" %i
            deleteFromFreeBlock(i)
    for i in allBlockList:
        if i not in freeBlockList:
            print "Block %d should  be in free block list" %i
            addTofreeblock(i)


def checkIndirect(blockNumber=26):
    currBlock = getData(blockNumber)
    if 'filename_to_inode_dict' in currBlock:
        for child in currBlock['filename_to_inode_dict']:
            if child == 'd:.' or child == 'd:..':
                continue
            checkIndirect(currBlock['filename_to_inode_dict'][child])
    else:
        f = open(FILE_PREFIX + str(currBlock['location']))
        content = f.read()
        f.close()
        contentChecker = content.split(',')
        try:
            for i in contentChecker:
                temp = int(i)
            if currBlock['indirect'] == 1 and len(contentChecker) == 1:
                #this error will be corrected when we check file size
                print "Block %d uses the indirect wrongly, length of location block list is 1 but indirect is 1" % blockNumber
            if currBlock['indirect'] == 0:
                print "Block %d's indirect should be 1" % blockNumber
                currBlock['indirect'] = 1
                writeToBlock(currBlock, blockNumber)
        except ValueError:
            if currBlock['indirect'] == 1:
                print "Block %d's indirect should be 0" % blockNumber
                currBlock['indirect'] = 0
                writeToBlock(currBlock, blockNumber)


def recoverFile(fileDict, blockNumber):
    try:
        locations = getData(fileDict['location'])
        content = ''
        counter = 0
        while counter < len(locations):
            f = open(FILE_PREFIX + str(locations[counter]))
            tempContent = f.read()
            f.close()
            content += tempContent
            counter += 1
        fileDict['indirect'] = 1
        fileDict['size'] = len(content)
        writeToBlock(fileDict, blockNumber)
    except:
        f = open(FILE_PREFIX + str(fileDict['location']))
        content = f.read()
        f.close()
        fileDict['size'] = len(content)
        fileDict['indirect'] = 0
        writeToBlock(fileDict, blockNumber)


def checkFileSize(blockNumber=26):
    currBlock = getData(blockNumber)
    if 'filename_to_inode_dict' in currBlock:
        for child in currBlock['filename_to_inode_dict']:
            if child == 'd:.' or child == 'd:..':
                continue
            checkFileSize(currBlock['filename_to_inode_dict'][child])
    else:
        if currBlock['indirect'] == 0 and (currBlock['size'] > 4096 or currBlock['size'] < 0):
            recoverFile(currBlock, blockNumber)
            print "Block %d's size is wrong" % blockNumber
        elif currBlock['indirect'] == 1:
            f = open(FILE_PREFIX + str(currBlock['location']))
            content = f.read()
            f.close()
            locationList = content.split(',')
            if len(locationList) == 1 and currBlock['size'] <= 4096:
                f = open(FILE_PREFIX + locationList[0])
                content = f.read()
                f.close()
                addTofreeblock(int(locationList[0]))
                currBlock['indirect'] = 0
                f = open(FILE_PREFIX + str(currBlock['location']), 'w')
                f.write(content)
                f.close()
                writeToBlock(currBlock, blockNumber)
            if currBlock['size'] <= ( len(locationList) - 1) * BLOCK_SIZE or currBlock['size'] > len(
                    locationList) * BLOCK_SIZE:
                print "Block %d's size is wrong" % blockNumber
                recoverFile(currBlock, blockNumber)


if __name__ == "__main__":
    print "This file checker will automatically correct all correctable errors which includes: device id error(will not be corrected), time error" \
          ", link count error, ./.. error, free block list error, indirect error, file size error(location block must be meaningful, contains block numbers or content)"
    print "If no warning appears during checking, there is no error within this file system"
    print "Start checking"
    checkDevID()
    checkTime()
    checkFreeBlockList()
    checkDir()
    checkIndirect()
    checkFileSize()
    print "End checking"
