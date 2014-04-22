/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/Obfuscate\040Directory\040Tree/RCS/ObfuscatorOfDirectoryTrees.cpp,v 1.3 2014/04/22 21:01:54 agmsmith Exp agmsmith $
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

/* Standard C++ library. */

#include <iostream>

/* BeOS (Be Operating System) headers. */

#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <NodeInfo.h>
#include <Path.h>
#include <String.h>


/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

#define PROGRAM_NAME "ObfuscatorOfDirectoryTrees"


/******************************************************************************
 * Global utility function to display an error message and return.  The message
 * part describes the error, and if ErrorNumber is non-zero, gets the string ",
 * error code $X (standard description)." appended to it.  If the message is
 * NULL then it gets defaulted to "Something went wrong".  The title part
 * doesn't get displayed.  The error gets printed to stderr.
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
ostream& PrintUsage (ostream& OutputStream)
{
  OutputStream << "\n" PROGRAM_NAME "\n";
  OutputStream << "Copyright © 2014 by Alexander G. M. Smith.\n";
  OutputStream << "Released to the public domain.\n\n";
  WrapTextToStream (OutputStream, "Compiled on " __DATE__ " at " __TIME__
".  $Revision: 1.3 $  $Header: /CommonBe/agmsmith/Programming/Obfuscate\040Directory\040Tree/RCS/ObfuscatorOfDirectoryTrees.cpp,v 1.3 2014/04/22 21:01:54 agmsmith Exp agmsmith $");
  OutputStream << "\n"
"This is a program for copying a directory tree to a new directory tree with\n"
"most of the identifying information obfuscated.  File and directory names,\n"
"file contents and so on are replaced by sequential numbers, mostly consisting\n"
"of leading zeroes so that the new value matches the length of the old value.\n"
"Attribute names are kept, but values are converted to sequential numbers.  The\n"
"sequential numbers are used so that a directory listing will be in the same\n"
"order as the original one.\n"
"\n"
"The original purpose of this program is to recreate a Haiku OS file system bug\n"
"with indexing of attributes.  Since the test data is personal e-mails, and is\n"
"too big to fit in a Zip file, obfuscating it while keeping the lengths of data\n"
"items the same should be sufficient for recreating the bug, as well as making\n"
"it compress really well.\n"
"\n"
"Usage: " PROGRAM_NAME " [-v] InputDir OutputDir\n"
"\n"
"-v for verbose mode, where it lists the directories and files copied.\n\n";

  return OutputStream;
}



/******************************************************************************
 * Finally, the main program which drives it all.
 */

int main (int argc, char**)
{
  if (argc <= 1)
    cout << PrintUsage; /* In case no arguments specified. */

  cerr << PROGRAM_NAME " shutting down..." << endl;
  return 0;
}
