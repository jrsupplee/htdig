//
// ExternalParser.cc
//
// ExternalParser: Implementation of ExternalParser
//                 Allows external programs to parse unknown document formats.
//                 The parser is expected to return the document in a 
//                 specific format. The format is documented 
//                 in http://www.htdig.org/attrs.html#external_parser
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Public License version 2 or later
// <http://www.gnu.org/copyleft/gpl.html>
//
// $Id: ExternalParser.cc,v 1.17 1999/09/29 16:33:12 loic Exp $
//

#include "ExternalParser.h"
#include "htdig.h"
#include "htString.h"
#include "QuotedStringList.h"
#include "URL.h"
#include "Dictionary.h"

#include <ctype.h>
#include <stdio.h>
#include "good_strtok.h"

static Dictionary	*parsers = 0;
extern String		configFile;

//*****************************************************************************
// ExternalParser::ExternalParser(char *contentType)
//
ExternalParser::ExternalParser(char *contentType)
{
    if (canParse(contentType))
    {
	currentParser = ((String *)parsers->Find(contentType))->get();
    }
    ExternalParser::contentType = contentType;
}


//*****************************************************************************
// ExternalParser::~ExternalParser()
//
ExternalParser::~ExternalParser()
{
}


//*****************************************************************************
// int ExternalParser::readLine(FILE *in, String &line)
//
int
ExternalParser::readLine(FILE *in, String &line)
{
    char	buffer[2048];
    int		length;
    
    line = 0;
    while (fgets(buffer, sizeof(buffer), in))
    {
	length = strlen(buffer);
	if (buffer[length - 1] == '\n')
	{
	    //
	    // A full line has been read.  Return it.
	    //
	    line << buffer;
	    line.chop('\n');
	    return 1;
	}
	else
	{
	    //
	    // Only a partial line was read.  Append it to the line
	    // and read some more.
	    //
	    line << buffer;
	}
    }
    return line.length() > 0;
}


//*****************************************************************************
// int ExternalParser::canParse(char *contentType)
//
int
ExternalParser::canParse(char *contentType)
{
    if (!parsers)
    {
	parsers = new Dictionary();
	
	QuotedStringList	qsl(config["external_parsers"], " \t");
	int			i;

	for (i = 0; qsl[i]; i += 2)
	{
	    parsers->Add(qsl[i], new String(qsl[i + 1]));
	}
    }
    return parsers->Exists(contentType);
}

//*****************************************************************************
// void ExternalParser::parse(Retriever &retriever, URL &base)
//
void
ExternalParser::parse(Retriever &retriever, URL &base)
{
    if (contents == 0 || contents->length() == 0 ||
	currentParser.length() == 0)
    {
	return;
    }

    //
    // Write the contents to a temporary file.
    //
    String      path = getenv("TMPDIR");
    if (path.length() == 0)
      path = "/tmp";
    path << "/htdext." << getpid();

    FILE	*fl = fopen(path, "w");
    if (!fl)
    {
	return;
    }
    
    fwrite(contents->get(), 1, contents->length(), fl);
    fclose(fl);
    
    //
    // Now start the external parser.
    //
    String	command = currentParser;
    command << ' ' << path << ' ' << contentType << " \"" << base.get() <<
	"\" " << configFile;

    FILE	*input = popen(command, "r");
    if (!input)
    {
	unlink(path);
	return;
    }

    unsigned int minimum_word_length
      = config.Value("minimum_word_length", 3);

    String	line;
    char	*token1, *token2, *token3;
    int		loc = 0, hd = 0;
    URL		url;
    while (readLine(input, line))
    {
	token1 = strtok(line, "\t");
	if (token1 == NULL)
	    token1 = "";
	token2 = NULL;
	token3 = NULL;
	switch (*token1)
	{
	    case 'w':	// word
		token1 = strtok(0, "\t");
		if (token1 != NULL)
		  token2 = strtok(0, "\t");
		if (token2 != NULL)
		  token3 = strtok(0, "\t");
		if (token1 != NULL && token2 != NULL && token3 != NULL &&
			(loc = atoi(token2)) >= 0 &&
			(hd = atoi(token3)) >= 0 && hd < 12)
		  retriever.got_word(token1, loc, hd);
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
		
	    case 'u':	// href
		token1 = strtok(0, "\t");
		if (token1 != NULL)
		  token2 = strtok(0, "\t");
		if (token1 != NULL && token2 != NULL)
		{
		  url.parse(token1);
		  url.hopcount(base.hopcount() + 1);
		  retriever.got_href(url, token2);
		}
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
		
	    case 't':	// title
		token1 = strtok(0, "\t");
		if (token1 != NULL)
		  retriever.got_title(token1);
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
		
	    case 'h':	// head
		token1 = strtok(0, "\t");
		if (token1 != NULL)
		  retriever.got_head(token1);
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
		
	    case 'a':	// anchor
		token1 = strtok(0, "\t");
		if (token1 != NULL)
		  retriever.got_anchor(token1);
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
		
	    case 'i':	// image url
		token1 = strtok(0, "\t");
		if (token1 != NULL)
		  retriever.got_image(token1);
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;

	    case 'm':	// meta
	      {
		// Using good_strtok means we can accept empty
		// fields.
		char *httpEquiv = good_strtok(token1+2, '\t');
		char *name = good_strtok(0, '\t');
		char *content = good_strtok(0, '\t');

		if (httpEquiv != NULL && name != NULL && content != NULL)
		{
		  // It would be preferable if we could share
		  // this part with HTML.cc, but it has other
		  // chores too, and I do not se a point where to
		  // split it up to get a common shared function
		  // (or class).  Which should not stop anybody from
		  // finding a better solution.
		  // For now, there is duplicated code.
		  static StringMatch *keywordsMatch = 0;
		  if (!keywordsMatch)
		  {
			StringList kn(config["keywords_meta_tag_names"], " \t");
			keywordsMatch = new StringMatch();
			keywordsMatch->IgnoreCase();
			keywordsMatch->Pattern(kn.Join('|'));
			kn.Release();
		  }
    
		  // <URL:http://www.w3.org/MarkUp/html-spec/html-spec_5.html#SEC5.2.5> 
		  // says that the "name" attribute defaults to
		  // the http-equiv attribute if empty.
		  if (*name == '\0')
		    name = httpEquiv;

		  if (*httpEquiv != '\0')
		  {
		    // <META HTTP-EQUIV=REFRESH case
		    if (mystrcasecmp(httpEquiv, "refresh") == 0
			&& *content != '\0')
		    {
		      char *q = (char*)mystrcasestr(content, "url=");
		      if (q && *q)
		      {
			q += 4; // skiping "URL="
			char *qq = q;
			while (*qq && (*qq != ';') && (*qq != '"') &&
			       !isspace(*qq))qq++;
			*qq = 0;
			URL href(q, base);
			// I don't know why anyone would do this, but hey...
			retriever.got_href(href, "");
		      }
		    }
		  }

		  //
		  // Now check for <meta name=...  content=...> tags that
		  // fly with any reasonable DTD out there
		  //
		  if (*name != '\0' && *content != '\0')
		  {
		    if (keywordsMatch->CompareWord(name))
		    {
		      char	*w = strtok(content, " ,\t\r");
		      while (w)
		      {
			if (strlen(w) >= minimum_word_length)
			  retriever.got_word(w, 1, 10);
			w = strtok(0, " ,\t\r");
		      }
		    }
		    else if (mystrcasecmp(name, "htdig-email") == 0)
		    {
		      retriever.got_meta_email(content);
		    }
		    else if (mystrcasecmp(name, "htdig-notification-date") == 0)
		    {
		      retriever.got_meta_notification(content);
		    }
		    else if (mystrcasecmp(name, "htdig-email-subject") == 0)
		    {
		      retriever.got_meta_subject(content);
		    }
		    else if (mystrcasecmp(name, "description") == 0 
			     && strlen(content) != 0)
		    {
		      //
		      // We need to do two things. First grab the description
		      //
		      String meta_dsc = content;

		      if (meta_dsc.length() > max_meta_description_length)
			meta_dsc = meta_dsc.sub(0, max_meta_description_length).get();
		      if (debug > 1)
			cout << "META Description: " << content << endl;
		      retriever.got_meta_dsc(meta_dsc);

		      //
		      // Now add the words to the word list
		      // (slot 11 is the new slot for this)
		      //
		      char	  *w = strtok(content, " \t\r");
		      while (w)
		      {
			if (strlen(w) >= minimum_word_length)
			  retriever.got_word(w, 1, 11);
			w = strtok(0, " \t\r");
		      }
		    }
		  }
		}
		else
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
	      }

	    default:
		  cerr<< "External parser error in line:"<<line<<"\n" << " URL: " << base.get() << "\n";
		break;
	}
    }
    pclose(input);
    unlink(path);
}


