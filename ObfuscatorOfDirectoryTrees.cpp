/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/Obfuscate\040Directory\040Tree/RCS/ObfuscatorOfDirectoryTrees.cpp,v 1.13 2014/04/25 15:23:25 agmsmith Exp $
 *
 * This is a BeOS program for obfuscating files and directories.  It
 * recursively copies the given file or directory to ones where all
 * identifiable information is changed to a squence of numbers.  So a 37 byte
 * long file name becomes a 37 digit number, with mostly leading zeroes.  The
 * same goes for attribute values and the file contents.  The end result should
 * be very compressable, suitable for uploading in a .zip file.
 *
 * Originally used for recreating Haiku OS file system bugs involved in copying
 * hundreds of thousands of e-mails without revealing personal data, and making
 * it small enough to fit in a Zip file.
 *
 * $Log: ObfuscatorOfDirectoryTrees.cpp,v $
 * Revision 1.13  2014/04/25 15:23:25  agmsmith
 * Oops, forgot to NUL terminate the string.
 *
 * Revision 1.12  2014/04/25 15:13:14  agmsmith
 * Test if obfuscated name already exists and try another one if it does,
 * can happen for short obfuscated names.
 *
 * Revision 1.11  2014/04/25 14:13:59  agmsmith
 * Truncate data (attributes and file contents) which are too big to fit
 * in memory (500MB limit).
 *
 * Revision 1.10  2014/04/25 01:02:10  agmsmith
 * Add a NUL byte at the end of string attributes, so they look better in the
 * attribute viewer.  Might affect compression a bit, but so what.
 *
 * Revision 1.9  2014/04/25 00:32:09  agmsmith
 * Now starting to work, does file data contents and recursion.
 *
 * Revision 1.8  2014/04/24 21:02:25  agmsmith
 * Added verbosity levels and dump data at the higher levels.
 *
 * Revision 1.7  2014/04/24 19:29:49  agmsmith
 * Now partly works - copying just the attributes of the initial directory,
 * and obfuscating them!  Now on to recursion...
 *
 * Revision 1.6  2014/04/23 21:57:37  agmsmith
 * Attribute copying under construction.
 *
 * Revision 1.5  2014/04/22 22:01:25  agmsmith
 * Now processes command line arguments, opens directories, and does nothing.
 *
 * Revision 1.4  2014/04/22 21:07:32  agmsmith
 * Wording improved.
 *
 * Revision 1.3  2014/04/22 21:01:54  agmsmith
 * Got rid of a lot of ANSI C++ warnings about new line inside strings,
 * convenient though it may be.
 *
 * Revision 1.2  2014/04/22 20:54:15  agmsmith
 * Now compiles!
 *
 * Revision 1.1  2014/04/22 20:45:43  agmsmith
 * Initial revision
 */

/* Standard C Library. */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>

/* Standard C++ library. */

#include <iostream>
#include <new> // For nothrow option when new'ing memory.

/* BeOS (Be Operating System) headers. */

#include <fs_attr.h>
#include <ByteOrder.h>
#include <Node.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <NodeInfo.h>
#include <Path.h>
#include <String.h>


/******************************************************************************
 * Global variables, and not-so-variable things too.
 */

#define PROGRAM_NAME "ObfuscatorOfDirectoryTrees"
static const int MAX_OBFUSCATE_BUFFER_SIZE = 500000000;
int gIndentLevel = 0;
long long int gSequenceNumber = 0;

enum eVerboseLevels
{
  VERBOSE_NONE = 0,
  VERBOSE_DIR,
  VERBOSE_FILE,
  VERBOSE_ATTR,
  VERBOSE_DATA,
  VERBOSE_EXTREME_DATA,
  VERBOSE_MAX
} gVerboseLevel = VERBOSE_NONE;


/******************************************************************************
 * Utility class to increment the indent level in its constructor and decrement
 * in the destructor.
 */

class AutoIndentIncrement
{
public:
  AutoIndentIncrement (int IncrementAmount = 1)
  {
    mIncrementAmount = IncrementAmount;
    gIndentLevel += mIncrementAmount;
  };

  ~AutoIndentIncrement ()
  {
    gIndentLevel -= mIncrementAmount;
  };

private:
  int mIncrementAmount;
};


/******************************************************************************
 * Global utility function to display an error message and return.  The message
 * part describes the error, and if ErrorNumber is non-zero, gets the string ",
 * error code $X (standard description)." appended to it.  If the message is
 * NULL then it gets defaulted to "Something went wrong".  The title part is
 * printed before the whole thing.  The text gets printed to stderr.
 */

static void DisplayErrorMessage (
  const char *MessageString = NULL,
  int ErrorNumber = 0,
  const char *TitleString = NULL)
{
  char ErrorBuffer [PATH_MAX + 1500];

  if (TitleString == NULL)
    TitleString = PROGRAM_NAME " Error Message";

  if (MessageString == NULL)
  {
    if (ErrorNumber == 0)
      MessageString = "No error, no message, why bother?";
    else
      MessageString = "Something went wrong";
  }

  if (ErrorNumber != 0)
  {
    sprintf (ErrorBuffer, "%s, error code $%X/%d (%s) has occured.",
      MessageString, ErrorNumber, ErrorNumber, strerror (ErrorNumber));
    MessageString = ErrorBuffer;
  }

  cerr << TitleString << ": " << MessageString << endl;
}



/******************************************************************************
 * Word wrap a long line of text into shorter 79 column lines and print the
 * result on the given output stream.
 */

static void WrapTextToStream (ostream& OutputStream, const char *TextPntr)
{
  const int LineLength = 79;
  char     *StringPntr;
  char      TempString [LineLength+1];

  TempString[LineLength] = 0; /* Only needs to be done once. */

  while (*TextPntr != 0)
  {
    while (isspace (*TextPntr))
      TextPntr++; /* Skip leading spaces. */
    if (*TextPntr == 0)
      break; /* It was all spaces, don't print any more. */

    strncpy (TempString, TextPntr, LineLength);

    /* Advance StringPntr to the end of the temp string, partly to see how long
    it is (rather than doing strlen). */

    StringPntr = TempString;
    while (*StringPntr != 0)
      StringPntr++;

    if (StringPntr - TempString < LineLength)
    {
      /* This line fits completely. */
      OutputStream << TempString << endl;
      TextPntr += StringPntr - TempString;
      continue;
    }

    /* Advance StringPntr to the last space in the temp string. */

    while (StringPntr > TempString)
    {
      if (isspace (*StringPntr))
        break; /* Found the trailing space. */
      else /* Go backwards, looking for the trailing space. */
        StringPntr--;
    }

    /* Remove more trailing spaces at the end of the line, in case there were
    several spaces in a row. */

    while (StringPntr > TempString && isspace (StringPntr[-1]))
      StringPntr--;

    /* Print the line of text and advance the text pointer too. */

    if (StringPntr == TempString)
    {
      /* This line has no spaces, don't wrap it, just split off a chunk. */
      OutputStream << TempString << endl;
      TextPntr += strlen (TempString);
      continue;
    }

    *StringPntr = 0; /* Cut off after the first trailing space. */
    OutputStream << TempString << endl;
    TextPntr += StringPntr - TempString;
  }
}


/******************************************************************************
 * Print the usage info to the stream.
 */

static ostream& PrintUsage (ostream& OutputStream)
{
  OutputStream << "\n" PROGRAM_NAME "\n\n";
  OutputStream << "Copyright © 2014 by Alexander G. M. Smith.\n";
  OutputStream << "Released to the public domain.\n\n";
  WrapTextToStream (OutputStream, "Compiled on " __DATE__ " at " __TIME__
".  $Revision: 1.13 $  $Header: /CommonBe/agmsmith/Programming/Obfuscate\040Directory\040Tree/RCS/ObfuscatorOfDirectoryTrees.cpp,v 1.13 2014/04/25 15:23:25 agmsmith Exp $");
  OutputStream << "\n"
"This is a program for copying a directory tree to a new directory tree with\n"
"most of the identifying information obfuscated.  File and directory names,\n"
"file contents and so on are replaced by sequential numbers, mostly consisting\n"
"of leading zeroes so that the new value matches the length of the old value,\n"
"up to a RAM buffer size limit of a few hundred megabytes.  Attribute names\n"
"are kept, but values are converted to sequential numbers.\n"
"\n"
"The original purpose of this program is to recreate a Haiku OS file system bug\n"
"with indexing of attributes.  Since the test data is personal e-mails, and is\n"
"too big to fit in a Zip file, obfuscating it while keeping the lengths of data\n"
"items the same should be sufficient for recreating the bug, as well as making\n"
"it compress really well.\n"
"\n"
"Usage: " PROGRAM_NAME " [-v|-vv|-vvv|-vvvv|-vvvvv] InputDir OutputDir\n"
"\n"
"-v for verbose mode, where more 'v's list more progress information.\n\n";

  return OutputStream;
}


/******************************************************************************
 * Print out a readable version of the given data buffer.  Hex dump plus
 * strings.  Optionally cuts off after a few hundred bytes.  Indents too.
 */

static void DumpBuffer (const char *pBuffer, int BufferSize)
{
  AutoIndentIncrement AutoIndenter;
  const char *pBufferEnd;
  const int BytesPerLine = 16;
  const int MaxPrintByteCount = 320;

  if (pBuffer == NULL || BufferSize <= 0)
    return;

  if (gVerboseLevel < VERBOSE_EXTREME_DATA &&
  BufferSize > MaxPrintByteCount)
    pBufferEnd = pBuffer + MaxPrintByteCount;
  else
    pBufferEnd = pBuffer + BufferSize;

  // Each byte printed uses 4 bytes of output: 2 for the hex digits, 1 for the
  // string value, and one space.  Format is:
  // 00 11 22 33 44 55 66 77 88 99 00 aa bb cc dd ee ff "0123456789abcdef"
  char OutputLine[BytesPerLine * 4 + 3];

  int LineBytes = 0;
  memset (OutputLine, ' ', sizeof (OutputLine) - 1);
  OutputLine[BytesPerLine * 3] = '"';
  OutputLine[BytesPerLine * 4 + 1] = '"';
  OutputLine[sizeof (OutputLine) - 1] = 0;
  while (true)
  {
    char TempBuf[4];
    unsigned char ByteValue = *pBuffer;
    sprintf (TempBuf, "%02X", ByteValue);
    OutputLine[LineBytes * 3] = TempBuf[0];
    OutputLine[LineBytes * 3 + 1] = TempBuf[1];
    if (ByteValue < 32)
      ByteValue = '_'; // Can't print control characters, use underscore.
    OutputLine[BytesPerLine * 3 + LineBytes + 1] = ByteValue;

    pBuffer++;
    LineBytes++;

    if (LineBytes >= BytesPerLine || pBuffer >= pBufferEnd)
    {
      printf ("%*s%s\n", gIndentLevel, "", OutputLine);
      LineBytes = 0;
      memset (OutputLine, ' ', sizeof (OutputLine) - 1);
      OutputLine[BytesPerLine * 3] = '"';
      OutputLine[BytesPerLine * 4 + 1] = '"';
      OutputLine[sizeof (OutputLine) - 1] = 0;
    }

    if (pBuffer >= pBufferEnd)
      break;
  }

  if (gVerboseLevel < VERBOSE_EXTREME_DATA && BufferSize > MaxPrintByteCount)
    printf ("%*s... and %d more bytes.\n", gIndentLevel, "",
      BufferSize - MaxPrintByteCount);
}


/******************************************************************************
 * Obfuscate the given buffer by filling it with the sequence number in ASCII
 * text form.  Adds as many leading zeros as needed to fill the whole buffer.
 * Result is not NUL terminated.
 *
 * Since this is running in 32 bit BeOS, the buffer can be at most about 1.8GB
 * in size, thus BufferSize is fine as an int.
 */

static void ObfuscateBuffer(char *pBuffer, int BufferSize)
{
  const int NumberLength = 23; // Max 64 bit number is about 20 digits.
  char NumberString[NumberLength + 1];

  if (pBuffer == NULL || BufferSize <= 0)
  {
    DisplayErrorMessage ("NULL or not postive size buffer inputs",
      B_BAD_VALUE, "ObfuscateBuffer");
    return;
  }

  memset (pBuffer, '0', BufferSize);
  sprintf (NumberString, "%0*Ld", NumberLength, gSequenceNumber++);

  // Copy as much of the number as will fit to the end of the buffer.

  int CopyLength = NumberLength;
  int StartPosition = BufferSize - NumberLength;
  if (StartPosition < 0)
  {
    CopyLength += StartPosition; // Reduces amount copied.
    StartPosition = 0;
  }
  memcpy (pBuffer + StartPosition, NumberString + (NumberLength - CopyLength),
    CopyLength);
}


/******************************************************************************
 * Copy the attributes from a source (file or directory) to a similar type of
 * destination.
 */

static status_t ObfuscateAttributes (BNode &SourceNode, BNode &DestNode)
{
  char AttributeName[B_ATTR_NAME_LENGTH+1];
  AutoIndentIncrement AutoIndenter;
  char ErrorMessage[B_ATTR_NAME_LENGTH+100];
  status_t ErrorNumber;

  ErrorNumber = SourceNode.RewindAttrs();
  if (ErrorNumber != B_OK)
  {
    DisplayErrorMessage ("Unable to rewind to first attribute", ErrorNumber,
      "ObfuscateAttributes");
    return ErrorNumber;
  }

  while (B_OK == (ErrorNumber = SourceNode.GetNextAttrName(AttributeName)))
  {
    struct attr_info AttributeInfo;
    ErrorNumber = SourceNode.GetAttrInfo(AttributeName, &AttributeInfo);
    if (ErrorNumber != B_OK)
    {
      sprintf (ErrorMessage, "Can't get info about \"%s\" attribute",
        AttributeName);
      DisplayErrorMessage (ErrorMessage, ErrorNumber, "ObfuscateAttributes");
      return ErrorNumber;
    }

    if (gVerboseLevel >= VERBOSE_ATTR)
    {
      char TypeString[5];
      * (uint32 *) TypeString = B_BENDIAN_TO_HOST_INT32(AttributeInfo.type);
      TypeString[4] = 0;

      printf ("%*sAttribute \"%s\" of type '%s', length %d.\n",
        gIndentLevel, "", AttributeName, TypeString, (int) AttributeInfo.size);
    }

    if (AttributeInfo.size < 0)
    {
      ErrorNumber = B_BAD_VALUE;
      sprintf (ErrorMessage, "Attribute \"%s\" has negative size %Ld",
        AttributeName, AttributeInfo.size);
      DisplayErrorMessage (ErrorMessage, ErrorNumber, "ObfuscateAttributes");
      return ErrorNumber;
    }

    if (AttributeInfo.size > MAX_OBFUSCATE_BUFFER_SIZE)
    {
      if (gVerboseLevel >= VERBOSE_ATTR)
      {
        AutoIndentIncrement AutoIndentMore;
        printf ("%*sTruncating attribute \"%s\" size from %Ld down to %d.\n",
          gIndentLevel, "", AttributeName, AttributeInfo.size,
          MAX_OBFUSCATE_BUFFER_SIZE);
      }
      AttributeInfo.size = MAX_OBFUSCATE_BUFFER_SIZE;
    }

    char *pData = new (std::nothrow) char [AttributeInfo.size];
    if (pData == NULL)
    {
      ErrorNumber = B_NO_MEMORY;
      sprintf (ErrorMessage,
        "Unable to allocate memory for attribute \"%s\" size %Ld",
        AttributeName, AttributeInfo.size);
      DisplayErrorMessage (ErrorMessage, ErrorNumber, "ObfuscateAttributes");
      return ErrorNumber;
    }

    if (gVerboseLevel >= VERBOSE_DATA)
    {
      ssize_t AmountRead = SourceNode.ReadAttr (AttributeName,
        AttributeInfo.type, 0 /* offset */, pData, AttributeInfo.size);
      if (AmountRead == AttributeInfo.size)
      {
        DumpBuffer (pData, AttributeInfo.size);
      }
      else
      {
        DisplayErrorMessage (AttributeName, AmountRead,
          "Unable to read attribute value (nonfatal - don't need data)");
      }
    }

    // For string type attributes, put the NUL back at the end of the
    // obfuscated string, otherwise it looks weird in the attribute viewer.

    if (AttributeInfo.size >= 1 && (AttributeInfo.type == B_MIME_STRING_TYPE ||
    AttributeInfo.type == B_STRING_TYPE))
    {
      ObfuscateBuffer (pData, AttributeInfo.size - 1);
      pData[AttributeInfo.size - 1] = 0;
    }
    else
      ObfuscateBuffer (pData, AttributeInfo.size);

    ssize_t AmountWritten = DestNode.WriteAttr (AttributeName,
      AttributeInfo.type, 0 /* offset */, pData, AttributeInfo.size);
    delete [] pData; // Get rid of buffer now, makes error handling easier.
    if (AmountWritten != AttributeInfo.size)
    {
      ErrorNumber = AmountWritten;
      if (ErrorNumber >= 0)
        ErrorNumber = B_IO_ERROR;
      sprintf (ErrorMessage, "Only wrote %d bytes of %d for \"%s\" attribute",
        (int) AmountWritten, (int) AttributeInfo.size, AttributeName);
      DisplayErrorMessage (ErrorMessage, ErrorNumber, "ObfuscateAttributes");
      return ErrorNumber;
    }

  } // end while GetNextAttrName

  if (ErrorNumber == B_ENTRY_NOT_FOUND)
    ErrorNumber = B_OK; // Reaching end of list isn't an error.
  else
    DisplayErrorMessage ("Problems reading attribute name list", ErrorNumber,
      "ObfuscateAttributes");

  return ErrorNumber;
}


/******************************************************************************
 * Given an already existing source file, create a destination one with
 * obfuscated contents.
 */

static status_t ObfuscateFile (BEntry &SourceEntry, BDirectory &DestDir,
  const char *DestName)
{
  AutoIndentIncrement AutoIndenter;
  char ErrorMessage[B_FILE_NAME_LENGTH+100];
  status_t ErrorNumber;

  char SourceName[B_FILE_NAME_LENGTH];
  SourceEntry.GetName (SourceName);

  BFile SourceFile (&SourceEntry, B_READ_ONLY);
  ErrorNumber = SourceFile.InitCheck();
  if (ErrorNumber != B_OK)
  {
    DisplayErrorMessage (SourceName, ErrorNumber,
      "ObfuscateFile: Unable to open file for reading");
    return ErrorNumber;
  }

  BFile DestFile;
  ErrorNumber = DestDir.CreateFile (DestName, &DestFile,
    true /* fail if exists */);
  if (ErrorNumber != B_OK)
  {
    DisplayErrorMessage (DestName, ErrorNumber,
      "ObfuscateFile: Unable to open file for writing");
    return ErrorNumber;
  }

  if (gVerboseLevel >= VERBOSE_FILE)
  {
    printf ("%*sFile \"%s\" is being obfuscated into \"%s\".\n",
      gIndentLevel, "", SourceName, DestName);
  }

  ErrorNumber = ObfuscateAttributes (SourceFile, DestFile);
  if (ErrorNumber != B_OK)
  {
    cerr << "Failed while obfuscating attributes of file \"" <<
      SourceName << "\".\n";
    return ErrorNumber;
  }

  off_t FileDataSize = 0;
  ErrorNumber = SourceFile.GetSize (&FileDataSize);
  if (ErrorNumber != B_OK)
  {
    DisplayErrorMessage (SourceName, ErrorNumber,
      "ObfuscateFile: Unable to get size of file");
    return ErrorNumber;
  }

  AutoIndentIncrement AutoIndentOneMore;

  if (gVerboseLevel >= VERBOSE_DATA)
  {
    printf ("%*sFile contents of length %d.\n", gIndentLevel, "",
      (int) FileDataSize);
  }

  if (FileDataSize > 0)
  {
    if (FileDataSize > MAX_OBFUSCATE_BUFFER_SIZE)
    {
      if (gVerboseLevel >= VERBOSE_FILE)
      {
        printf ("%*sTruncating file \"%s\" size from %Ld down to %d.\n",
          gIndentLevel, "", SourceName, FileDataSize,
          MAX_OBFUSCATE_BUFFER_SIZE);
      }
      FileDataSize = MAX_OBFUSCATE_BUFFER_SIZE;
    }

    char *pFileData = new (std::nothrow) char [FileDataSize];
    if (pFileData == NULL)
    {
      ErrorNumber = B_NO_MEMORY;
      sprintf (ErrorMessage,
        "Unable to allocate memory for file \"%s\" data size %Ld",
        SourceName, FileDataSize);
      DisplayErrorMessage (ErrorMessage, ErrorNumber, "ObfuscateFile");
      return ErrorNumber;
    }

    if (gVerboseLevel >= VERBOSE_DATA)
    {
      ssize_t AmountRead = SourceFile.Read (pFileData, FileDataSize);
      if (AmountRead == FileDataSize)
      {
        DumpBuffer (pFileData, FileDataSize);
      }
      else
      {
        DisplayErrorMessage (SourceName, AmountRead,
          "Unable to read file contents (nonfatal - don't need data)");
      }
    }

    ObfuscateBuffer(pFileData, FileDataSize);

    ssize_t AmountWritten = DestFile.Write (pFileData, FileDataSize);
    delete [] pFileData; // Get rid of buffer now, makes error handling easier.
    if (AmountWritten != FileDataSize)
    {
      ErrorNumber = AmountWritten;
      if (ErrorNumber >= 0)
        ErrorNumber = B_IO_ERROR;
      sprintf (ErrorMessage, "Only wrote %d bytes of %d for file \"%s\" data",
        (int) AmountWritten, (int) FileDataSize, DestName);
      DisplayErrorMessage (ErrorMessage, ErrorNumber, "ObfuscateFile");
      return ErrorNumber;
    }
  }
  return B_OK;
}


/******************************************************************************
 * Given an already existing source and destination directory, copy/obfuscate
 * all the files and directories within it.
 */

static status_t ObfuscateDirectory (BDirectory &SourceDir, BDirectory &DestDir)
{
  status_t ErrorNumber = 0;

  BPath DestPath (&DestDir, ".");
  BPath SourcePath (&SourceDir, ".");

  if (gVerboseLevel >= VERBOSE_DIR)
  {
    printf ("%*sDirectory \"%s\" is being obfuscated into \"%s\".\n",
      gIndentLevel, "", SourcePath.Path(), DestPath.Path());
  }

  ErrorNumber = ObfuscateAttributes (SourceDir, DestDir);
  if (ErrorNumber != B_OK)
  {
    cerr << "Failed while obfuscating attributes of directory \"" <<
      SourcePath.Path() << "\".\n";
    return ErrorNumber;
  }

  SourceDir.Rewind();

  BEntry CurSourceEntry;
  char CurDestName[B_FILE_NAME_LENGTH];
  char CurSourceName[B_FILE_NAME_LENGTH];
  struct stat CurSourceStat;

  while (B_OK == (ErrorNumber = SourceDir.GetNextEntry(&CurSourceEntry)))
  {
    ErrorNumber = CurSourceEntry.GetName (CurSourceName);
    if (ErrorNumber != B_OK)
    {
      DisplayErrorMessage (SourcePath.Path(), ErrorNumber,
        "ObfuscateDirectory: Problems getting contained item name");
      return ErrorNumber;
    }

    // Generate a new obfuscated name.  If it collides with existing obfuscated
    // names, try a few other names.  Can happen with short names, indeed there
    // are only 10 possible one letter names.  If there are too many
    // collisions, try a longer name, though that breaks the obfuscation rules.

    int GenerateNameRetryCount;
    for (GenerateNameRetryCount = 0; GenerateNameRetryCount < 48;
    GenerateNameRetryCount++)
    {
      int NewLength = strlen (CurSourceName) + (GenerateNameRetryCount >> 3);
      if (NewLength >= B_FILE_NAME_LENGTH)
        NewLength = B_FILE_NAME_LENGTH - 1;

      ObfuscateBuffer (CurDestName, NewLength);
      CurDestName[NewLength] = 0;

      if (!DestDir.Contains(CurDestName))
        break; // Doesn't contain the new name, safe to use it.

      if (gVerboseLevel > VERBOSE_NONE)
      {
        AutoIndentIncrement AutoIndentMore;

        printf ("%*sName \"%s\" already exists in directory \"%s\", "
          "will try another possibly longer name.\n", gIndentLevel, "",
          CurDestName, DestPath.Path());
      }
    }

    ErrorNumber = CurSourceEntry.GetStat(&CurSourceStat);
    if (ErrorNumber != B_OK)
    {
      DisplayErrorMessage (CurSourceName, ErrorNumber,
        "ObfuscateDirectory: Problems reading entry status");
      return ErrorNumber;
    }

    if (S_ISREG(CurSourceStat.st_mode))
    {
      ErrorNumber = ObfuscateFile (CurSourceEntry, DestDir, CurDestName);
    }
    else if (S_ISDIR(CurSourceStat.st_mode))
    {
      BDirectory SubDestDir;
      ErrorNumber = DestDir.CreateDirectory (CurDestName, &SubDestDir);
      if (ErrorNumber != B_OK)
      {
        DisplayErrorMessage (CurDestName, ErrorNumber,
          "ObfuscateDirectory: Failed to create destination directory");
        // Fall through for directory level error message.
      }
      else
      {
        AutoIndentIncrement AutoIndenter;
        BDirectory SubSourceDir (&CurSourceEntry);
        ErrorNumber = ObfuscateDirectory (SubSourceDir, SubDestDir);
      }
    }
    else if (S_ISLNK(CurSourceStat.st_mode))
    {
      if (gVerboseLevel >= VERBOSE_FILE)
        printf ("%*sSymbolic link \"%s\" will be ignored.\n",
          gIndentLevel, "", CurSourceName);
    }
    else
    {
      if (gVerboseLevel >= VERBOSE_FILE)
        printf ("%*sHard link or other unknown file system entity "
          "\"%s\" will be ignored.\n", gIndentLevel, "", CurSourceName);
    }

    if (ErrorNumber != B_OK)
    {
      cerr << "ObfuscateDirectory failed while converting item \"" <<
        CurSourceName << "\" in directory \"" << SourcePath.Path() <<
        "\" into item \"" << CurDestName << "\" in directory \"" <<
        DestPath.Path() << "\".\n";
      return ErrorNumber;
    }
  }

  if (ErrorNumber == B_ENTRY_NOT_FOUND)
    ErrorNumber = B_OK; // Reaching end of list isn't an error.
  else
  {
    DisplayErrorMessage (SourcePath.Path(), ErrorNumber,
      "ObfuscateDirectory: Problems reading directory entries");
  }
  return B_OK;
}


/******************************************************************************
 * Finally, the main program which drives it all.
 */

int main (int argc, char** argv)
{
  BDirectory DestDir;
  status_t ErrorNumber;
  BDirectory SourceDir;

  enum ArgStateEnum {ASE_LOOKING_FOR_SOURCE, ASE_LOOKING_FOR_DEST, ASE_DONE}
    eArgState = ASE_LOOKING_FOR_SOURCE;

  int iArg;
  for (iArg = 1; iArg < argc; iArg++)
  {
    char ErrorMessage [1024];

    if (strlen(argv[iArg]) > sizeof (ErrorMessage) - 100)
      cerr << "Argument is too long, ignoring it: " << argv[iArg] << endl;
    else if (strcmp(argv[iArg], "-v") == 0)
      gVerboseLevel = VERBOSE_DIR;
    else if (strcmp(argv[iArg], "-vv") == 0)
      gVerboseLevel = VERBOSE_FILE;
    else if (strcmp(argv[iArg], "-vvv") == 0)
      gVerboseLevel = VERBOSE_ATTR;
    else if (strcmp(argv[iArg], "-vvvv") == 0)
      gVerboseLevel = VERBOSE_DATA;
    else if (strcmp(argv[iArg], "-vvvvv") == 0)
      gVerboseLevel = VERBOSE_EXTREME_DATA;
    else if (eArgState == ASE_LOOKING_FOR_SOURCE)
    {
      ErrorNumber = SourceDir.SetTo(argv[iArg]);
      if (ErrorNumber != B_OK)
      {
        sprintf(ErrorMessage,
          "Unable to open source directory \"%s\"", argv[iArg]);
        DisplayErrorMessage (ErrorMessage, ErrorNumber, "Main");
        break;
      }
      eArgState = ASE_LOOKING_FOR_DEST;
    }
    else if (eArgState == ASE_LOOKING_FOR_DEST)
    {
      ErrorNumber = DestDir.SetTo(argv[iArg]);
      if (ErrorNumber == B_ENTRY_NOT_FOUND)
      {
        ErrorNumber = create_directory(argv[iArg], 0777);
        if (ErrorNumber == B_OK)
        {
          if (gVerboseLevel >= VERBOSE_DIR)
            cout << "Created destination directory \"" << argv[iArg] << "\"\n";
        }
        else
        {
          sprintf(ErrorMessage,
            "Unable to create destination directory \"%s\"", argv[iArg]);
          DisplayErrorMessage (ErrorMessage, ErrorNumber, "Main");
          break;
        }
        ErrorNumber = DestDir.SetTo(argv[iArg]);
      }
      if (ErrorNumber != B_OK)
      {
        sprintf(ErrorMessage,
          "Unable to open destination directory \"%s\"", argv[iArg]);
        DisplayErrorMessage (ErrorMessage, ErrorNumber, "Main");
        break;
      }
      eArgState = ASE_DONE;
    }
  }

  if (eArgState != ASE_DONE)
  {
    cerr << "Insufficient number of valid arguments provided.\n";
    PrintUsage(cout);
    ErrorNumber = -1;
  }
  else
  {
    if (gVerboseLevel > VERBOSE_NONE)
    {
      static const char* VerboseNames[VERBOSE_MAX] = {
        "None", "Directory", "File", "Attribute", "Data", "Extreme Data"};
      printf ("Starting obfuscation, verbosity level '%s'.\n",
        VerboseNames[gVerboseLevel]);
    }
    ErrorNumber = ObfuscateDirectory (SourceDir, DestDir);
  }

  if (gVerboseLevel > VERBOSE_NONE)
  {
    cerr << PROGRAM_NAME " finished, return code " << ErrorNumber << ".\n";
  }

  return ErrorNumber; // Zero for success.
}
