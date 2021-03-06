// Matt Wells, copyright Jul 2001

// . used to parse XML/HTML (romantic char set) into words
// . TODO: ensure WordType m_types[] array is only 1 byte per entry
// . ??? a word should end at any non-alnum ??? then using phrasing for "tim's"

#ifndef GB_WORDS_H
#define GB_WORDS_H

#include "max_words.h"
#include "nodeid_t.h"
#include "utf8_fast.h"
#include <inttypes.h>

// now Matches.h has 300 Words classes handy... try to do away with this
// make sure it does not slow us down!!
#define WORDS_LOCALBUFSIZE 80

class Xml;


class Words {

 public:

	// . set words from a string
	// . s must be NULL terminated
	// . NOTE: we never own the data
	// . there is typically no html in "s"
	// . html tags are NOT parsed out
	bool set(const char *s);

	// . similar to above
	// . but we temporarily stick a \0 @ s[slen] for parsing purposes
	bool set(const char *s, int32_t slen);

	// . new function to set directly from an Xml, rather than extracting
	//   text first
	// . use range (node1,node2] and if node2 is -1 that means the last one
	bool set(Xml *xml, int32_t node1 = 0, int32_t node2 = -1);

	bool addWords(const char *s, int32_t nodeLen);

	// get the spam modified score of the ith word (baseScore is the 
	// score if the word is not spammed)
	int32_t getNumWords() const {
		return m_numWords;
	}
	int32_t getNumAlnumWords() const {
		return m_numAlnumWords;
	}
	const char *getWord( int32_t n ) const {
		return m_words[n];
	}
	int32_t getWordLen( int32_t n ) const {
		return m_wordLens[n];
	}
	int32_t getNumTags() const {
		return m_numTags;
	}

	// . size of string from word #a up to and NOT including word #b
	// . "b" can be m_numWords to mean up to the end of the doc
	int32_t getStringSize( int32_t a, int32_t b ) const {
		// do not let it exceed this
		if ( b >= m_numWords ) {
			b = m_numWords;
		}

		// pedal it back. we might equal a then. which is ok, that
		// means to just return the length of word #a then
		b--;

		if ( b < a ) {
			return 0;
		}

		if ( a < 0 ) {
			return 0;
		}

		int32_t size = m_words[b] - m_words[a];

		// add in size of word #b
		size += m_wordLens[b];

		return size;
	}

	// . CAUTION: don't call this for punct "words"... it's bogus for them
	// . this is only for alnum "words"
	int64_t getWordId( int32_t n ) const {
		return m_wordIds [n];
	}

	// . how many quotes in the nth word?
	// . how many plusses in the nth word?
	// . used exclusively by Query class for parsing query syntax
	int32_t getNumQuotes( int32_t n ) const {
		int32_t count = 0;
		for ( int32_t i = 0; i < m_wordLens[n]; i++ ) {
			if ( m_words[n][i] == '\"' ) {
				count++;
			}
		}

		return count;
	}

	// . do we have a ' ' 't' '\n' or '\r' in this word?
	// . caller should not call this is isPunct(n) is false, pointless.

	bool hasSpace( int32_t n ) const {
		for ( int32_t i = 0; i < m_wordLens[n]; i++ ) {
			if ( is_wspace_utf8( &m_words[n][i] ) ) {
				return true;
			}
		}

		return false;
	}

	bool hasChar( int32_t n, char c ) const {
		for ( int32_t i = 0; i < m_wordLens[n]; i++ ) {
			if ( m_words[n][i] == c ) {
				return true;
			}
		}

		return false;
	}

	bool isSpaces( int32_t n, int32_t starti = 0 ) const {
		for ( int32_t i = starti; i < m_wordLens[n]; i++ ) {
			if ( !is_wspace_utf8( &m_words[n][i] ) ) {
				return false;
			}
		}
		return true;
	}

	// if this is set from xml, every word is either a word or an xml node
	nodeid_t getTagId( int32_t n ) const {
		if ( !m_tagIds ) {
			return 0;
		}

		return ( m_tagIds[n] & BACKBITCOMP );
	}

	bool isBackTag( int32_t n ) const {
		if ( !m_tagIds ) {
			return false;
		}

		if ( m_tagIds[n] & BACKBIT ) {
			return true;
		}

		return false;
	}

	// CAUTION!!!
	//
	// "BACKBIT" is set in the tagid  of m_tagIds[] to indicate the tag is
	// a "back tag" as opposed to a "front tag". i.e. </a> vs. <a>
	// respectively. so mask it out by doing "& BACKBITCOMP" if you just
	// want the pure tagid!!!!
	//
	// CAUTION!!!
	const nodeid_t *getTagIds() const { return m_tagIds; }
	const char     * const *getWordPtrs() const { return (const char*const*)m_words; }
	const int32_t  *getWordLens() const { return m_wordLens; }
	const int64_t  *getWordIds() const { return m_wordIds; }
	const int32_t  *getNodes() const { return m_nodes; }
	
	// 2 types of "words": punctuation and alnum
	// isPunct() will return true on tags, too, so they are "punct"
	bool      isPunct  ( int32_t n ) const { return m_wordIds[n] == 0;}
	bool      isAlnum  ( int32_t n ) const { return m_wordIds[n] != 0;}
	bool      isAlpha  ( int32_t n ) const { 
		if ( m_wordIds[n] == 0LL ) return false;
		if ( isNum ( n )         ) return false;
		return true;
	}

	bool      isNum    ( int32_t n ) const { 
		if ( ! is_digit(m_words[n][0]) ) return false;
		const char *p    = m_words[n];
		const char *pend = p + m_wordLens[n];
		for (  ; p < pend ; p++ )
			if ( ! is_digit(*p) ) return false;
		return true;
	}

	// . are all alpha char capitalized?
	bool      isUpper  ( int32_t n ) const {
		// skip if not alnum...
		if ( m_wordIds[n] == 0LL ) {
			return false;
		}
		return is_upper_utf8_string(m_words[n], m_words[n]+m_wordLens[n]);
	}

	bool isCapitalized( int32_t n ) const {
		if ( !is_alpha_utf8( m_words[n] ) ) {
			return false;
		}

		return is_upper_utf8( m_words[n] );
		//todo: handle titlecase letters (
	}

	 Words     ( );
	~Words     ( );
	void reset ( ); 

private:

	bool allocateWordBuffers(int32_t count, bool tagIds = false);
	
	char  m_localBuf [ WORDS_LOCALBUFSIZE ];

	char *m_localBuf2;
	int32_t  m_localBufSize2;

	char *m_buf;
	int32_t  m_bufSize;

	int32_t           m_preCount  ; // estimate of number of words in the doc
	const char          **m_words    ;  // pointers to the word
	int32_t           *m_wordLens ;  // length of each word
	int64_t      *m_wordIds  ;  // lower ascii hash of word
	int32_t           *m_nodes    ;  // Xml.cpp node # (for tags only)
	nodeid_t       *m_tagIds   ;  // tag for xml "words"

 	int32_t           m_numWords;      // # of words we have
	int32_t           m_numAlnumWords;

	int32_t m_numTags;
};

#endif // GB_WORDS_H
