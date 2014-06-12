#ifndef GBINJECT_H
#define GBINJECT_H    

bool sendPageInject ( class TcpSocket *s, class HttpRequest *hr );

#include "XmlDoc.h"
#include "Users.h"
#include "Parms.h" // GigablastRequest

class Msg7 {

public:

	GigablastRequest m_gr;

	bool       m_needsSet;
	XmlDoc     m_xd;
	TcpSocket *m_socket;
	SafeBuf    m_sb;
	char m_round;
	char m_useAhrefs;
	HashTableX m_linkDedupTable;

	void *m_state;
	void (* m_callback )(void *state);

	//long m_crawlbotAPI;

	Msg7 ();
	~Msg7 ();

	bool scrapeQuery ( );

	bool inject ( void *state ,
		      void (*callback)(void *state) );

};

extern bool g_inPageInject;

#endif
