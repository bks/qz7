PROG = 7zxr.dll
DEF_FILE = ../../Archive/Archive2.def
LIBS = $(LIBS) user32.lib oleaut32.lib
CFLAGS = $(CFLAGS) -I ../../../ \
  -DEXTRACT_ONLY \
  -DCOMPRESS_MT \
  -D_NO_CRYPTO

COMMON_OBJS = \
  $O\CRC.obj \
  $O\IntToString.obj \
  $O\NewHandler.obj \
  $O\MyString.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\MyVector.obj \
  $O\Wildcard.obj \

WIN_OBJS = \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\PropVariant.obj \
  $O\Synchronization.obj \
  $O\System.obj \

7ZIP_COMMON_OBJS = \
  $O\CreateCoder.obj \
  $O\InBuffer.obj \
  $O\InOutTempBuffer.obj \
  $O\FilterCoder.obj \
  $O\LimitedStreams.obj \
  $O\LockedStream.obj \
  $O\MethodId.obj \
  $O\MethodProps.obj \
  $O\OutBuffer.obj \
  $O\ProgressUtils.obj \
  $O\StreamBinder.obj \
  $O\StreamObjects.obj \
  $O\StreamUtils.obj \
  $O\VirtThread.obj \

AR_OBJS = \
  $O\ArchiveExports.obj \
  $O\DllExports2.obj \

AR_COMMON_OBJS = \
  $O\CoderMixer2.obj \
  $O\CoderMixer2MT.obj \
  $O\CrossThreadProgress.obj \
  $O\HandlerOut.obj \
  $O\ItemNameUtils.obj \
  $O\OutStreamWithCRC.obj \
  $O\ParseProperties.obj \


7Z_OBJS = \
  $O\7zCompressionMode.obj \
  $O\7zDecode.obj \
  $O\7zExtract.obj \
  $O\7zFolderOutStream.obj \
  $O\7zHandler.obj \
  $O\7zHeader.obj \
  $O\7zIn.obj \
  $O\7zProperties.obj \
  $O\7zRegister.obj \


COMPRESS_OBJS = \
  $O\CodecExports.obj \

SWAP_OPT_OBJS = \
  $O\ByteSwap.obj \
  $O\ByteSwapRegister.obj \

BRANCH_OPT_OBJS = \
  $O\x86.obj \
  $O\x86_2.obj \
  $O\BranchCoder.obj \
  $O\BranchMisc.obj \
  $O\BranchRegister.obj \
  $O\BCJRegister.obj \
  $O\BCJ2Register.obj \

COPY_OBJS = \
  $O\CopyCoder.obj \
  $O\CopyRegister.obj \

LZMA_OPT_OBJS = \
  $O\LZMADecoder.obj \
  $O\LZMARegister.obj \

C_OBJS = \
  $O\7zCrc.obj \
  $O\Alloc.obj \
  $O\Bra.obj \
  $O\Bra86.obj \
  $O\BraIA64.obj \
  $O\LzmaDec.obj \
  $O\Threads.obj \

OBJS = \
  $O\StdAfx.obj \
  $(CONSOLE_OBJS) \
  $(COMMON_OBJS) \
  $(WIN_OBJS) \
  $(7ZIP_COMMON_OBJS) \
  $(AR_OBJS) \
  $(AR_COMMON_OBJS) \
  $(7Z_OBJS) \
  $(COMPRESS_OBJS) \
  $(BRANCH_OPT_OBJS) \
  $(SWAP_OPT_OBJS) \
  $(COPY_OBJS) \
  $(LZMA_OPT_OBJS) \
  $(C_OBJS) \
  $O\resource.res


!include "../../../Build.mak"

$(COMMON_OBJS): ../../../Common/$(*B).cpp
	$(COMPL)
$(WIN_OBJS): ../../../Windows/$(*B).cpp
	$(COMPL)
$(7ZIP_COMMON_OBJS): ../../Common/$(*B).cpp
	$(COMPL)
$(AR_OBJS): ../../Archive/$(*B).cpp
	$(COMPL)
$(AR_COMMON_OBJS): ../../Archive/Common/$(*B).cpp
	$(COMPL)

$(7Z_OBJS): ../../Archive/7z/$(*B).cpp
	$(COMPL)

$(COMPRESS_OBJS): ../../Compress/$(*B).cpp
	$(COMPL)
$(BRANCH_OPT_OBJS): ../../Compress/Branch/$(*B).cpp
	$(COMPL_O2)
$(SWAP_OPT_OBJS): ../../Compress/ByteSwap/$(*B).cpp
	$(COMPL_O2)
$(COPY_OBJS): ../../Compress/Copy/$(*B).cpp
	$(COMPL)
$(LZMA_OPT_OBJS): ../../Compress/LZMA/$(*B).cpp
	$(COMPL_O2)

$(C_OBJS): ../../../../C/$(*B).c
	$(COMPL_O2)
