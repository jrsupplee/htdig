//
// htpurge.cc
//
// htpurge: A utility to remove specified URLs and any documents
// marked for removal from the word and document databases.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2000 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Public License version 2 or later
// <http://www.gnu.org/copyleft/gpl.html>
//
// $Id: htpurge.cc,v 1.1.2.1 2000/03/19 03:58:41 ghutchis Exp $
//

#include "WordContext.h"
#include "HtWordReference.h"
#include "HtConfiguration.h"
#include "DocumentDB.h"
#include "DocumentRef.h"
#include "defaults.h"
#include "HtURLCodec.h"

#include <errno.h>
#include <unistd.h>

// If we have this, we probably want it.
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

//
// This hash is used to keep track of all the document IDs which have to be
// discarded.
// This is generated from the doc database and is used to prune words
// from the word db
//
Dictionary	discard_list;

int		verbose = 0;

void purgeDocs();
void purgeWords();
void usage();
void reportError(char *msg);

//*****************************************************************************
// int main(int ac, char **av)
//
int main(int ac, char **av)
{
    int			alt_work_area = 0;
    String		configfile = DEFAULT_CONFIG_FILE;
    int			c;
    extern char		*optarg;

    while ((c = getopt(ac, av, "vc:a")) != -1)
    {
	switch (c)
	{
	    case 'c':
		configfile = optarg;
		break;
	    case 'v':
		verbose++;
		break;
	    case 'a':
		alt_work_area++;
		break;
	    case '?':
		usage();
		break;
	}
    }

    config.Defaults(&defaults[0]);

    if (access((char*)configfile, R_OK) < 0)
    {
	reportError(form("Unable to find configuration file '%s'",
			 configfile.get()));
    }
	
    config.Read(configfile);

    //
    // Check url_part_aliases and common_url_parts for
    // errors.
    String url_part_errors = HtURLCodec::instance()->ErrMsg();

    if (url_part_errors.length() != 0)
      reportError(form("Invalid url_part_aliases or common_url_parts: %s",
                       url_part_errors.get()));

    if (alt_work_area != 0)
    {
	String	configValue;

	configValue = config["word_db"];
	if (configValue.length() != 0)
	{
	    configValue << ".work";
	    config.Add("word_db", configValue);
	}

	configValue = config["doc_db"];
	if (configValue.length() != 0)
	{
	    configValue << ".work";
	    config.Add("doc_db", configValue);
	}

	configValue = config["doc_index"];
	if (configValue.length() != 0)
	{
	    configValue << ".work";
	    config.Add("doc_index", configValue);
	}

	configValue = config["doc_excerpt"];
	if (configValue.length() != 0)
	{
	    configValue << ".work";
	    config.Add("doc_excerpt", configValue);
	}
    }

    WordContext::Initialize(config);

    purgeDocs();
    purgeWords();

    return 0;
}

//*****************************************************************************
// void purgeDocs()
//
void purgeDocs()
{
    const String	doc_db = config["doc_db"];
    const String	doc_index = config["doc_index"];
    const String	doc_excerpt = config["doc_excerpt"];
    int			remove_unused = config.Boolean("remove_bad_urls");
    int			remove_unretrieved = config.Boolean("remove_unretrieved_urls");
    DocumentDB		db;
    List		*IDs;
    int			document_count = 0;

    //
    // Start the conversion by going through all the URLs that are in
    // the document database
    //
    if(db.Open(doc_db, doc_index, doc_excerpt) != OK)
      return;
    
    IDs = db.DocIDs();
	
    IDs->Start_Get();
    IntObject		*id;
    String		idStr;
    String		url;
    while ((id = (IntObject *) IDs->Get_Next()))
    {
	DocumentRef	*ref = db[id->Value()];

	if (!ref)
	    continue;

	db.ReadExcerpt(*ref);
	url = ref->DocURL();
	idStr = 0;
	idStr << id->Value();

	if (ref->DocState() == Reference_noindex)
	  {
	    // This document either wasn't found or shouldn't be indexed.
	    db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, noindex ID: " << idStr << " URL: "
                   << url << endl;
	    discard_list.Add(idStr.get(), NULL);
	  }
	else if (ref->DocState() == Reference_obsolete)
	  {
	    // This document was replaced by a newer one
	    db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, obsolete ID: " << idStr << " URL: "
                   << url << endl;
	    discard_list.Add(idStr.get(), NULL);
	  }
	else if (remove_unused && ref->DocState() == Reference_not_found)
	  {
	    // This document wasn't actually found
	    db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, not found ID: " << idStr << " URL: "
                   << url << endl;
	    discard_list.Add(idStr.get(), NULL);
	  }
	else if (remove_unused && strlen(ref->DocHead()) == 0 
		 && ref->DocAccessed() != 0)
	  {
	    // For some reason, this document was retrieved, but doesn't
	    // have an excerpt (probably because of a noindex directive)
	    db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, no excerpt ID: " << idStr << " URL:  "
                   << url << endl;
	    discard_list.Add(idStr.get(), NULL);
	  }
	else if (remove_unretrieved && ref->DocAccessed() == 0)
	  {
	    // This document has not been retrieved
	    db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, never retrieved ID: " << idStr << " URL:  "
                   << url << endl;
	    discard_list.Add(idStr.get(), NULL);
	  }
	else
	  {
	    // This is a valid document. Let's keep stats on it.
            if (verbose > 1)
              cout << "ID: " << idStr << " URL: " << url << endl;

	    document_count++;
	    if (verbose && document_count % 10 == 0)
	    {
		cout << "htpurge: " << document_count << '\n';
		cout.flush();
	    }
	  }
        delete ref;
    }
    if (verbose)
	cout << "\n";

    delete IDs;
    db.Close();
}

//
// Callback data dedicated to Dump and dump_word communication
//
class DeleteWordData : public Object
{
public:
  DeleteWordData(const Dictionary& discard_arg) : discard(discard_arg) { deleted = remains = 0; }

  const Dictionary& discard;
  int deleted;
  int remains;
};

//*****************************************************************************
//
//
static int delete_word(WordList *words, WordDBCursor& cursor, const WordReference *word_arg, Object &data)
{
  const HtWordReference *word = (const HtWordReference *)word_arg;
  DeleteWordData& d = (DeleteWordData&)data;
  static String docIDStr;

  docIDStr = 0;
  docIDStr << word->DocID();

  if(d.discard.Exists(docIDStr)) {
    if(words->Delete(cursor) != 1) {
      cerr << "htmerge: deletion of " << *word << " failed " << strerror(errno) << "\n";
      return NOTOK;
    }
    if (verbose)
      {
	cout << "htmerge: Discarding ";
	if(verbose > 2)
	  cout << *word;
	else 
	  cout << word->Word();
	cout << "\n";
	cout.flush();
      }
    d.deleted++;
  } else {
    d.remains++;
  }

  return OK;
}

//*****************************************************************************
// void purgeWords()
//
void purgeWords()
{
  HtWordList		words(config);
  DeleteWordData	data(discard_list); 

  words.Open(config["word_db"], O_RDWR);
  WordSearchDescription search(delete_word, &data);
  words.Walk(search);
  
  words.Close();

  if (verbose)
    cout << "\n";

}

//*****************************************************************************
// void usage()
//   Display program usage information
//
void usage()
{
    cout << "usage: htpurge [-v][-a][-c configfile]\n";
    cout << "This program is part of ht://Dig " << VERSION << "\n\n";
    cout << "Options:\n";
    cout << "\t-v\tVerbose mode.  This increases the verbosity of the\n";
    cout << "\t\tprogram.  Using more than 2 is probably only useful\n";
    cout << "\t\tfor debugging purposes.  The default verbose mode\n";
    cout << "\t\tgives a progress on what it is doing and where it is.\n\n";
    cout << "\t-a\tUse alternate work files.\n";
    cout << "\t\tTells htmerge to append .work to database files causing\n";
    cout << "\t\ta second copy of the database to be built.  This allows\n";
    cout << "\t\toriginal files to be used by htsearch during the indexing\n";
    cout << "\t\trun.\n\n";
    cout << "\t-c configfile\n";
    cout << "\t\tUse the specified configuration file instead on the\n";
    cout << "\t\tdefault.\n\n";
    exit(0);
}


//*****************************************************************************
// Report an error and die
//
void reportError(char *msg)
{
    cout << "htpurge: " << msg << "\n\n";
    exit(1);
}