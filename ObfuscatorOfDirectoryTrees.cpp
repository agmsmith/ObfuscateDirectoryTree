/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/AGMSBayesianSpam/Server/RCS/AGMSBayesianSpamServer.cpp,v 1.74 2002/12/14 02:43:57 agmsmith Exp $
 *
 * This is a BeOS program for obfuscating files and directories.  It
 * recursively copies the given file or directory to ones where all
 * identifiable information is changed to a squence of numbers.  So a 37 byte
 * long file name becomes a 37 digit number, with mostly leading zeroes.  The
 * same goes for attribute values and the file contents.  The end result should
 * be very compressable, suitable for uploading in a .zip file.
 *
 * $Log: AGMSBayesianSpamServer.cpp,v $
 */

/* Standard C Library. */

#include <stdio.h>
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

static void WrapTextToStream (ostream& OutputStream, char *TextPntr)
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
".  $Revision: 1.74 $  $Header: /CommonBe/agmsmith/Programming/AGMSBayesianSpam/Server/RCS/AGMSBayesianSpamServer.cpp,v 1.74 2002/12/14 02:43:57 agmsmith Exp $");
  OutputStream << "
This is a program for copying a directory tree to a new directory tree with
most of the identifying information obfuscated.  File and directory names, file
contents and so on are replaced by sequential numbers, mostly consisting of
leading zeroes so that the new value matches the length of the old value.
Attribute names are kept, but values are converted to sequential numbers.  The
sequential numbers are used so that a directory listing will be in the same
order as the original one.

The original purpose of this program is to recreate a Haiku OS file system bug
with indexing of attributes.  Since the test data is personal e-mails, and is
too big to fit in a Zip file, obfuscating it while keeping the lengths of data
items the same should be sufficient for recreating the bug.

Usage: " PROGRAM_NAME " InputDir OutputDir\n"

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
