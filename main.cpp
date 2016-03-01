//
// Matt Wells, copyright Sep 2001
// 

#include "gb-include.h"

#include <sched.h>        // clone()
// declare this stuff up here for call the pread() in our seek test below
//
// maybe we should put this in a common header file so we don't have 
// certain files compiled with the platform default, and some not -partap

#include "Version.h" // getVersion()
#include "Mem.h"
#include "Conf.h"
#include "Threads.h"
#include "Hostdb.h"
#include "Indexdb.h"
#include "Posdb.h"
#include "Datedb.h"
#include "Titledb.h"
#include "Tagdb.h"
#include "Spider.h"
#include "SpiderColl.h"
#include "SpiderLoop.h"
#include "Doledb.h"
#include "Clusterdb.h"
#include "Sections.h"
#include "Statsdb.h"
#include "UdpServer.h"
#include "PingServer.h"
#include "Repair.h"
#include "DailyMerge.h"
#include "MsgC.h"
#include "HttpServer.h"
#include "Loop.h"
#include "HighFrequencyTermShortcuts.h"
#include "IPAddressChecks.h"
#include <sys/resource.h>  // setrlimit
#include "Stats.h"
#include "Speller.h"       // g_speller
#include "Wiki.h"          // g_wiki
#include "Wiktionary.h"    // g_wiktionary
#include "CountryCode.h"
#include "Pos.h"
#include "Title.h"
#include "Speller.h"

// include all msgs that have request handlers, cuz we register them with g_udp
#include "Msg0.h"
#include "Msg1.h"
#include "Msg4.h"
#include "Msg13.h"
#include "Msg20.h"
#include "Msg22.h"
#include "Msg39.h"
#include "Msg40.h"    // g_resultsCache
#include "Msg17.h"
#include "Parms.h"
#include "Pages.h"
#include "Unicode.h"

#include "Msg1f.h"
#include "Profiler.h"
#include "Blaster.h"
#include "Proxy.h"

#include "linkspam.h"
#include "Process.h"
#include "sort.h"
#include "RdbBuckets.h"
#include "Test.h"
#include "SpiderProxy.h"
#include "HashTable.h"

// call this to shut everything down
bool mainShutdown ( bool urgent ) ;
//bool mainShutdown2 ( bool urgent ) ;

bool registerMsgHandlers ( ) ;
bool registerMsgHandlers1 ( ) ;
bool registerMsgHandlers2 ( ) ;
bool registerMsgHandlers3 ( ) ;
// makes a default conf file and saves into confFilename
//void makeNewConf ( int32_t hostId , char *confFilename );

void allExitWrapper ( int fd , void *state ) ;

void rmTest();

int g_inMemcpy=0;

static void dumpTitledb  (char *coll, int32_t sfn, int32_t numFiles, bool includeTree,
			   int64_t docId , bool justPrintDups );
static int32_t dumpSpiderdb ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree,
			   char printStats , int32_t firstIp );

static void dumpTagdb( char *coll, int32_t sfn, int32_t numFiles, bool includeTree, char rec = 0,
					   int32_t rdbId = RDB_TAGDB, char *site = NULL );

static void dumpIndexdb  ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree, 
			   int64_t termId ) ;
void dumpPosdb  ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree, 
		  int64_t termId , bool justVerify ) ;
static void dumpWaitingTree( char *coll );
static void dumpDoledb  ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree);

void dumpDatedb   ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree, 
		    int64_t termId , bool justVerify ) ;
void dumpClusterdb       ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree);

//void dumpStatsdb 	 ( int32_t startFileNum, int32_t numFiles, bool includeTree,
//			   int test );
			   
void dumpLinkdb          ( char *coll,int32_t sfn,int32_t numFiles,bool includeTree,
			   char *url );

void exitWrapper ( void *state ) { exit(0); };

bool g_recoveryMode = false;

int32_t g_recoveryLevel = 0;
	
bool isRecoveryFutile ( ) ;

int copyFiles ( char *dstDir ) ;


char *getcwd2 ( char *arg ) ;

// for cleaning up indexdb
void dumpMissing ( char *coll );
void dumpDups    ( char *coll );
void removeDocIds ( char *coll , char *filename );

static void dumpIndexdbFile ( int32_t fn , int64_t off , char *f , int32_t ks ,
			      char *NAME = NULL );

static int32_t checkDirPerms ( char *dir ) ;

// benchmark RdbTree::addRecord() for indexdb
bool treetest    ( ) ;
bool bucketstest ( char *dbname ) ;
bool hashtest    ( ) ;
// how fast to parse the content of this docId?
bool parseTest ( char *coll , int64_t docId , char *query );
bool summaryTest1   ( char *rec, int32_t listSize, char *coll , int64_t docId ,
		      char *query );
//bool summaryTest2   ( char *rec, int32_t listSize, char *coll , int64_t docId ,
//		      char *query );
//bool summaryTest3   ( char *rec, int32_t listSize, char *coll , int64_t docId ,
//		      char *query );

// time a big write, read and then seeks
bool thrutest ( char *testdir , int64_t fileSize ) ;
void seektest ( char *testdir , int32_t numThreads , int32_t maxReadSize ,
		char *filename );

bool pingTest ( int32_t hid , uint16_t clientPort );
bool memTest();
bool cacheTest();
bool ramdiskTest();
void countdomains( char* coll, int32_t numRecs, int32_t verb, int32_t output );

UdpProtocol g_dp; // Default Proto

// installFlag konstants 
typedef enum {
	ifk_install = 1,
	ifk_start ,
	ifk_installgb ,
	ifk_installgbrcp ,
	ifk_installconf ,
	ifk_gendbs ,
	ifk_genclusterdb ,
	ifk_distributeC ,
	ifk_installgb2 ,
	ifk_dsh ,
	ifk_dsh2 ,
	ifk_backupcopy ,
	ifk_backupmove ,
	ifk_backuprestore ,
	ifk_proxy_start ,
	ifk_installconf2 ,
	ifk_kstart ,
	ifk_dstart ,
	ifk_dumpmissing ,
	ifk_removedocids ,
	ifk_dumpdups ,
	ifk_tmpstart ,
	ifk_installtmpgb ,
	ifk_proxy_kstart ,
	ifk_start2 	
} install_flag_konst_t;

static int install_file(const char *file);
int install ( install_flag_konst_t installFlag , int32_t hostId , 
	      char *dir = NULL , char *coll = NULL , int32_t hostId2 = -1 , 
	      char *cmd = NULL );
int scale   ( char *newhostsconf , bool useShotgunIp );
int collinject ( char *newhostsconf );
int collcopy ( char *newHostsConf , char *coll , int32_t collnum ) ;

bool doCmd ( const char *cmd , int32_t hostId , char *filename , bool sendToHosts,
	     bool sendToProxies, int32_t hostId2=-1 );
int injectFile ( char *filename , char *ips , 
		 //int64_t startDocId ,
		 //int64_t endDocId ,
		 //bool isDelete ) ;
		 char *coll );
int injectFileTest ( int32_t  reqLen  , int32_t hid ); // generates the file
void membustest ( int32_t nb , int32_t loops , bool readf ) ;

//void tryMergingWrapper ( int fd , void *state ) ;

void saveRdbs ( int fd , void *state ) ;
bool shutdownOldGB ( int16_t port ) ;
//void resetAll ( );
//void spamTest ( ) ;

extern void resetPageAddUrl    ( );
extern void resetHttpMime      ( );
extern void reset_iana_charset ( );
extern void resetAdultBit      ( );
extern void resetDomains       ( );
extern void resetEntities      ( );
extern void resetQuery         ( );
extern void resetStopWords     ( );
extern void resetUnicode       ( );

extern void tryToSyncWrapper ( int fd , void *state ) ;

int main2 ( int argc , char *argv[] ) ;

int main ( int argc , char *argv[] ) {
	int ret = main2 ( argc , argv );

	if ( ret ) {
		fprintf(stderr,"Failed to start gb. Exiting.\n");
	}
}

int main2 ( int argc , char *argv[] ) {
	g_conf.m_runAsDaemon = false;
	g_conf.m_logToFile = false;

#ifndef CYGWIN
	// appears that linux 2.4.17 kernel would crash with this?
	// let's try again on gk127 to make sure
	// YES! gk0 cluster has run for months with this just fine!!
	mlockall(MCL_CURRENT|MCL_FUTURE);
#endif

	//g_timedb.makeStartKey ( 0 );

	// record time for uptime
	g_stats.m_uptimeStart = time(NULL);

	// malloc test for efence
	//char *ff = (char *)mmalloc(100,"efence");
	//ff[100] = 1;

	if (argc < 0) {
	printHelp:
		SafeBuf sb;
		sb.safePrintf(
			      "\n"
			      "Usage: gb [CMD]\n");
		sb.safePrintf(
			      "\n"
			      "\tgb will first try to load "
			      "the hosts.conf in the same directory as the "
			      "gb binary. "
			      "Then it will determine its hostId based on "
			      "the directory and IP address listed in the "
			      "hosts.conf file it loaded. Things in []'s "
			      "are optional.");
		/*
		sb.safePrintf(
			      "\n\t"
			 "[hostsConf] is the hosts.conf config file as "
			 "described in overview.html. If not provided then "
			 "it is assumed to be ./hosts.conf. If "
			      "./localhosts.conf exists then that will be "
			      "used instead of ./hosts.conf. That is "
			      "convenient to use since it will not be "
			      "overwritten from git pulls.\n\n" );
		*/
		sb.safePrintf(
			"[CMD] can have the following values:\n\n"

			"-h\tPrint this help.\n\n"
			"-v\tPrint version and exit.\n\n"

			//"<hostId>\n"
			//"\tstart the gb process for this <hostId> locally."
			//" <hostId> is 0 to run as host #0, for instance."
			//"\n\n"


			//"<hostId> -d\n\trun as daemon.\n\n"
			"-d\tRun as daemon.\n\n"

			//"-o\tprint the overview documentation in HTML. "
			//"Contains the format of hosts.conf.\n\n"

			// "<hostId> -r\n\tindicates recovery mode, "
			// "sends email to addresses "
			// "specified in Conf.h upon startup.\n\n"
			// "-r\tindicates recovery mode, "
			// "sends email to addresses "
			// "specified in Conf.h upon startup.\n\n"

			"start [hostId]\n"
			"\tStart the gb process on all hosts or just on "
			"[hostId], if specified, using an ssh command. Runs "
			"each gb process in a keepalive loop under bash.\n\n"

			"start <hostId1-hostId2>\n"
			"\tLike above but just start gb on the supplied "
			"range of hostIds.\n\n"

			"dstart [hostId]\n"
			"\tLike above but do not use a keepalive loop. So "
			"if gb crashes it will not auto-resstart.\n\n"

			/*
			"kstart [hostId]\n"
			"\tstart the gb process on all hosts or just on "
			"[hostId] if specified using an ssh command and "
			"if the gb process cores then restart it. k stands "
			"for keepalive.\n\n"
			*/

			"stop [hostId]\n"
			"\tSaves and exits for all gb hosts or "
			"just on [hostId], if specified.\n\n"

			"stop <hostId1-hostId2>\n"
			"\tTell gb to save and exit on the given range of "
			"hostIds.\n\n"

			"save [hostId]\n"
			"\tJust saves for all gb hosts or "
			"just on [hostId], if specified.\n\n"


			/*
			"tmpstart [hostId]\n"
			"\tstart the gb process on all hosts or just on "
			"[hostId] if specified, but "
			"use the ports specified in hosts.conf PLUS one. "
			"Then you can switch the "
			"proxy over to point to those and upgrade the "
			"original cluster's gb. "
			"That can be done in the Master Controls of the "
			"proxy using the 'use "
			"temporary cluster'. Also, this assumes the binary "
			"name is tmpgb not gb.\n\n"

			"tmpstop [hostId]\n"
			"\tsaves and exits for all gb hosts or "
			"just on [hostId] if specified, for the "
			"tmpstart command.\n\n"
			*/

			"spidersoff [hostId]\n"
			"\tDisables spidering for all gb hosts or "
			"just on [hostId], if specified.\n\n"

			"spiderson [hostId]\n"
			"\tEnables spidering for all gb hosts or "
			"just on [hostId], if specified.\n\n"

			/*
			"cacheoff [hostId]\n"
			"\tdisables all disk PAGE caches on all hosts or "
			"just on [hostId] if specified.\n\n"

			"freecache [maxShmid]\n"
			"\tfinds and frees all shared memory up to shmid "
			"maxShmid, default is 3000000.\n\n"
			*/

			/*
			"ddump [hostId]\n"
			"\tdump all b-trees in memory to sorted files on "
			"disk. "
			"Will likely trigger merges on files on disk. "
			"Restrict to just host [hostId] if given.\n\n"
			*/

			/*
			"pmerge [hostId|hostId1-hostId2]\n"
			"\tforce merge of posdb files "
			"just on [hostId] if specified.\n\n"

			"smerge [hostId|hostId1-hostId2]\n"
			"\tforce merge of sectiondb files "
			"just on [hostId] if specified.\n\n"

			"tmerge [hostId|hostId1-hostId2]\n"
			"\tforce merge of titledb files "
			"just on [hostId] if specified.\n\n"

			"merge [hostId|hostId1-hostId2]\n"
			"\tforce merge of all rdb files "
			"just on [hostId] if specified.\n\n"
			*/

			"dsh <CMD>\n"
			"\tRun this command on the primary IPs of "
			"all active hosts in hosts.conf. It will be "
			"executed in the gigablast working directory on "
			"each host. Example: "
			"gb dsh 'ps auxw; uptime'\n\n"

			/*
			"dsh2 <CMD>\n"
			"\trun this command on the secondary IPs of "
			"all active hosts in hosts.conf. Example: "
			"gb dsh2 'ps auxw; uptime'\n\n"
			*/

			"install [hostId]\n"
			"\tInstall all required files for gb from "
			"current working directory of the gb binary "
			"to [hostId]. If no [hostId] is specified, install "
			"to ALL hosts.\n\n"

			/*
			"install2 [hostId]\n"
			"\tlike above, but use the secondary IPs in the "
			"hosts.conf.\n\n"
			*/

			"installgb [hostId]\n"
			"\tLike above, but install just the gb executable.\n\n"

			"installgbrcp [hostId]\n"
			"\tLike above, but install just the gb executable "
			"and using rcp.\n\n"

			"installfile <file>\n"
			"\tInstalls the specified file on all hosts\n\n"

			/*
			"installgb2 [hostId]\n"
			"\tlike above, but use the secondary IPs in the "
			"hosts.conf.\n\n"

			"installtmpgb [hostId]\n"
			"\tlike above, but install just the gb executable "
			"as tmpgb (for tmpstart).\n\n"
			*/
			"installconf [hostId]\n"
			"\tlike above, but install hosts.conf and gb.conf\n\n"
			/*
			"installconf2 [hostId]\n"
			"\tlike above, but install hosts.conf and gbN.conf "
			"to the secondary IPs.\n\n"

			"backupcopy <backupSubdir>\n"
			"\tsave a copy of all xml, config, data and map files "
			"into <backupSubdir> which is relative "
			"to the working dir. Done for all hosts.\n\n"

			"backupmove <backupSubdir>\n"
			"\tmove all all xml, config, data and map files "
			"into <backupSubdir> which  is relative "
			"to the working dir. Done for all hosts.\n\n"

			"backuprestore <backupSubdir>\n"
			"\tmove all all xml, config, data and map files "
			"in <backupSubdir>,  which is relative "
			"to the working dir, into the working dir. "
			"Will NOT overwrite anything. Done for all "
			"hosts.\n\n"
			
			"proxy start [proxyId]\n"
			"\tStart a proxy that acts as a frontend to gb "
			"and passes on requests to random machines on "
			"the cluster given in hosts.conf. Helps to "
			"distribute the load evenly across all machines.\n\n"

			"proxy load <proxyId>\n"
			"\tStart a proxy process directly without calling "
			"ssh. Called by 'gb proxy start'.\n\n"

			"proxy stop [proxyId]\n"
			"\tStop a proxy that acts as a frontend to gb.\n\n"

			"blasterdiff [-v] [-j] [-p] <file1> <file2> "
			"<maxNumThreads> <wait>\n"
			"\tcompare search results between urls in file1 and"
			"file2 and output the search results in the url"
			" from file1 not found in the url from file2 "
			"maxNumThreads is the number of concurrent "
			"comparisons "
			"that should be done at one time and wait is the"
			"time to wait between comparisons.  -v is for "
			"verbose "
			" and -j is to just display links not found and "
			"not "
			"search for them on server2. If you do not want to"
			" use the proxy server "
			"on gk10, use -p\n\n"
			*/

			/*
			"blaster [-l|-u|-i] <file> <maxNumThreads> <wait>\n"
			"\tget documents from the urls given in file. The "
			"-l argument is to "
			"automatically get documents "
			"from the gigablast log file.\n"
			"\t-u means to inject/index the url into gb.\n"
			"\t-i means to inject/index the url into gb AND "
			"add all of its outlinks to\n"
			"\tspiderdb for spidering, "
			"which also entails a DNS lookup on each outlink.\n"
			"\tmaxNumThreads is the"
			" number of concurrent threads at one time and wait "
			" is the time to wait between threads.\n\n"
			*/

			/*
			"scale <newHosts.conf>\n"
			"\tGenerate a script to be called to migrate the "
			"data to the new places. Remaining hosts will "
			"keep the data they have, but it will be "
			"filtered during the next merge operations.\n\n"

			"collcopy <newHosts.conf> <coll> <collnum>\n"
			"\tGenerate a script to copy the collection data on "
			"the cluster defined by newHosts.conf to the "
			"current cluster. Remote network must have "
			"called \"gb ddump\" twice in a row just before to "
			"ensure all of its data is on disk.\n\n"
			*/


			// gb inject <file> <ip:port> [startdocid]
			// gb inject titledb <newhosts.conf> [startdocid]
			"inject <filename> "
			"<ip:port> [collection]\n"
			"\tInject all documents in <filename> into the gb "
			"host at ip:port. File must be in WARC format. "
			"Uses collection of 'main' if not specified. If "
			"ip:port is a hosts.conf file then a round-robin "
			"approach will be used."
			// "Each document listed in the file "
			// "must be preceeded by a valid HTTP mime with "
			// "a Content-Length: field. WARC files are also ok."
			"\n\n"

			/*
			"inject titledb-<DIR> <newhosts.conf> [startdocid]\n"
			"\tInject all pages from all the titledb "
			"files in the <DIR> directory into the appropriate "
			"host defined by the newhosts.conf config file. This "
			"is useful for populating one search engine with "
			"another. "
			"\n\n"

			"injecttest <requestLen> [hostId]\n"
			"\tinject random documents into [hostId]. If [hostId] "
			"not given 0 is assumed.\n\n"

			"ping <hostId> [clientport]\n"
			"\tperforms pings to <hostId>. [clientport] defaults "
			"to 2050.\n\n"
			*/

			/*
			"spellcheck <file>\n"
			"\tspellchecks the the queries in <file>.\n\n"

			"dictlookuptest <file>\n"
			"\tgets the popularities of the entries in the "
			"<file>. Used to only check performance of "
			"getPhrasePopularity.\n\n"

			// less common things
			"gendict <coll> [numWordsToDump]\n\tgenerate "
			"dictionary used for spellchecker "
			"from titledb files in collection <coll>. Use "
			"first [numWordsToDump] words.\n\n"
			//"gendbs <coll> [hostId]\n\tgenerate missing spiderdb "
			//"files from titledb files.\n\n"

			//"genclusterdb <coll> [hostId]\n\tgenerate missing "
			//"clusterdb.\n\n"

			//"gendaterange <coll> [hostId]\n\tgenerate missing "
			//"date range terms in all title recs.\n\n"

			//"update\tupdate titledb0001.dat\n\n"
			"treetest\n\ttree insertion speed test\n\n"

			"bucketstest [dbname]\n\tcompare speed and accuracy of "
			"buckets vs tree in add, getList and deleteList.  "
			"With an argument, test validity of db's saved buckets\n\n"

			"hashtest\n\tadd and delete into hashtable test\n\n"

			"parsetest <docIdToTest> [coll] [query]\n\t"
			"parser speed tests\n\n"
			*/

			/*
			"thrutest [dir] [fileSize]\n\tdisk write/read speed "
			"test\n\n"

			"seektest [dir] [numThreads] [maxReadSize] "
			"[filename]\n"
			"\tdisk seek speed test\n\n"
			
			"memtest\n"
			"\t Test how much memory we can use\n\n"
			*/

			/*
			// Quality Tests
			"countdomains <coll> <X>\n"
			"\tCounts the domains and IPs in collection coll and "
			"in the first X titledb records.  Results are sorted"
			"by popularity and stored in the log file. \n\n"

			"cachetest\n\t"
			"cache stability and speed tests\n\n"

			"ramdisktest\n\t"
			"test ramdisk functionality\n\n"

			"xmldiff [-td] <file1> <file2>\n"
			"\tTest xml diff routine on file1 and file2.\n"
			"\t-t: only show diffs in tag structure.\n"
			"\t-d: run as daemon.\n"
			"\n"

			"dump e <coll> <UTCtimestamp>\n\tdump all events "
			"as if the time is UTCtimestamp.\n\n"

			"dump es <coll> <UTCtimestamp>\n\tdump stats for "
			"all events as if the time is UTCtimestamp.\n\n"
			*/

			"dump <db> <collection>\n\tDump a db from disk. "
			"Example: gb dump t main\n"
			"\t<collection> is the name of the collection.\n"

			"\t<db> is s to dump spiderdb."
			//"set [T] to 1 to print "
			//"new stats. 2 to print old stats. "
			//"T is ip of firstip."
			"\n"

			"\t<db> is t to dump titledb. "
			//"\tT is the first docId to dump. Applies only to "
			//"titledb. "
			"\n"

			"\t<db> is p to dump posdb (the index)."
			//"\tOptional: T is the termid to dump."
			"\n"

			"\t<db> is D to dump duplicate docids in titledb.\n"
			"\t<db> is S to dump tagdb.\n"
			"\t<db> is W to dump tagdb for wget.\n"
			"\t<db> is x to dump doledb.\n"
			"\t<db> is w to dump waiting tree.\n"
			"\t<db> is C to dump catdb.\n"
			"\t<db> is l to dump clusterdb.\n"
			"\t<db> is z to dump statsdb all keys.\n"
			"\t<db> is Z to dump statsdb all keys and "
			"data samples.\n"
			"\t<db> is L to dump linkdb.\n"
			);
		SafeBuf sb2;
		sb2.brify2 ( sb.getBufStart() , 60 , "\n\t" , false );
		fprintf(stdout,"%s",sb2.getBufStart());
		// disable printing of used memory
		g_mem.m_used = 0;
		return 0;
	}

	int32_t  cmdarg = 0;

	// get command

	// it might not be there, might be a simple "./gb" 
	char *cmd = "";
	if ( argc >= 2 ) {
		cmdarg = 1;
		cmd = argv[1];
	}

	char *cmd2 = "";
	if ( argc >= 3 )
		cmd2 = argv[2];

	int32_t arch = 64;
	if ( sizeof(char *) == 4 ) arch = 32;

	// help
	if ( strcmp ( cmd , "-h" ) == 0 ) goto printHelp;
	// version
	if ( strcmp ( cmd , "-v" ) == 0 ) {
		printVersion();
		return 0; 
	}

	//send an email on startup for -r, like if we are recovering from an
	//unclean shutdown.
	g_recoveryMode = false;
	char *cc = NULL;
	if ( strncmp ( cmd , "-r" ,2 ) == 0 ) cc = cmd;
	if ( strncmp ( cmd2 , "-r",2 ) == 0 ) cc = cmd2;
	if ( cc ) {
		g_recoveryMode = true;
		g_recoveryLevel = 1;
		if ( cc[2] ) g_recoveryLevel = atoi(cc+2);
		if ( g_recoveryLevel < 0 ) g_recoveryLevel = 0;
	}

	// run as daemon? then we have to fork
	if ( strcmp ( cmd , "-d" ) == 0 ) g_conf.m_runAsDaemon = true;
	if ( strcmp ( cmd2 , "-d" ) == 0 ) g_conf.m_runAsDaemon = true;

	if ( strcmp ( cmd , "-l" ) == 0 ) g_conf.m_logToFile = true;
	if ( strcmp ( cmd2 , "-l" ) == 0 ) g_conf.m_logToFile = true;

	bool testMandrill = false;
	if ( strcmp ( cmd , "emailmandrill" ) == 0 ) {
		testMandrill = true;
	}

	// gb gendbs, preset the hostid at least
	if ( //strcmp ( cmd , "gendbs"   ) == 0 ||
	     strcmp ( cmd , "dumpmissing" ) == 0 ||
	     strcmp ( cmd , "dumpdups" ) == 0 ||
	     //strcmp ( cmd , "gencatdb" ) == 0 ||
	     //strcmp ( cmd , "genclusterdb" ) == 0 ||
	     //strcmp ( cmd , "gendaterange" ) == 0 || 
	     strcmp ( cmd , "distributeC" ) == 0 ) {
		// ensure we got a collection name after the cmd
		if ( cmdarg + 2 >  argc ) goto printHelp;
		// may also have an optional hostid
		//if ( cmdarg + 3 == argc ) hostId = atoi ( argv[cmdarg+2] );
	}

	if( (strcmp( cmd, "countdomains" ) == 0) &&  (argc >= (cmdarg + 2)) ) {
		uint32_t tmp = atoi( argv[cmdarg+2] );
		if( (tmp * 10) > g_mem.m_memtablesize )
		g_mem.m_memtablesize = tmp * 10;
	}

	// set it for g_hostdb and for logging
	//g_hostdb.m_hostId = hostId;

	// these tests do not need a hosts.conf
	/*
	if ( strcmp ( cmd , "trietest" ) == 0 ) {
		trietest();
		return 0;
	}
	if (strcmp ( cmd, "matchertest" ) == 0 ) {
		matchertest(argc - 2, argv + 2);
		return 0;
	}
	*/

	if ( strcmp ( cmd , "bucketstest" ) == 0 ) {
		if ( argc > cmdarg+1 ) bucketstest(argv[cmdarg+1]);
		else if( argc == cmdarg+1 ) bucketstest(NULL);
		else goto printHelp;
		return 0;
	}

	// these tests do not need a hosts.conf
	if ( strcmp ( cmd , "treetest" ) == 0 ) {
		if ( argc > cmdarg+1 ) goto printHelp;
		treetest();
		return 0;
	}
	// these tests do not need a hosts.conf
	if ( strcmp ( cmd , "hashtest" ) == 0 ) {
		if ( argc > cmdarg+1 ) goto printHelp;
		hashtest();
		return 0;
	}
	// these tests do not need a hosts.conf
	if ( strcmp ( cmd , "memtest" ) == 0 ) {
		if ( argc > cmdarg+1 ) goto printHelp;
		memTest();
		return 0;
	}
	if ( strcmp ( cmd , "cachetest" ) == 0 ) {
		if ( argc > cmdarg+1 ) goto printHelp;
		cacheTest();
		return 0;
	}
	if ( strcmp ( cmd , "ramdisktest" ) == 0 ) {
		if ( argc > cmdarg+1 ) goto printHelp;
		ramdiskTest();
		return 0;
	}
	if ( strcmp ( cmd , "parsetest"  ) == 0 ) {
		if ( cmdarg+1 >= argc ) goto printHelp;
		// load up hosts.conf
		//if ( ! g_hostdb.init(hostId) ) {
		//	log("db: hostdb init failed." ); return 1; }
		// init our table for doing zobrist hashing
		if ( ! hashinit() ) {
			log("db: Failed to init hashtable." ); return 1; }

		int64_t docid = atoll1(argv[cmdarg+1]);
		char *coll   = "";
		char *query  = "";
		if ( cmdarg+3 <= argc ) coll  = argv[cmdarg+2];
		if ( cmdarg+4 == argc ) query = argv[cmdarg+3];
		parseTest( coll, docid, query );
		return 0;
	}

	if ( strcmp ( cmd , "booltest" ) == 0 ){
		//if ( ! g_hostdb.init(hostId) ) {
		//	log("db: hostdb init failed." ); return 1; }
		// init our table for doing zobrist hashing
		if ( ! hashinit() ) {
			log("db: Failed to init hashtable." ); return 1; }
		if (!ucInit(g_hostdb.m_dir)) {
			log("Unicode initialization failed!");
			return 1;
		}
		//testBoolean();
		return 0;
		
	}

	/*
	if ( strcmp ( cmd , "querytest" ) == 0){
		if ( ! g_hostdb.init(hostsConf, hostId) ) {
			log("db: hostdb init failed." ); return 1; }
		// init our table for doing zobrist hashing
		if ( ! hashinit() ) {
			log("db: Failed to init hashtable." ); return 1; }
		if (!ucInit(g_hostdb.m_dir)) {
			log("Unicode initialization failed!");
			return 1;
		}
		queryTest();
		return 0;
		
	}
	*/

	if ( strcmp ( cmd ,"isportinuse") == 0 ) {
		if ( cmdarg+1 >= argc ) goto printHelp;
		int port = atol ( argv[cmdarg+1] );
		// make sure port is available. returns false if in use.
		if ( ! g_httpServer.m_tcp.testBind(port,false) )
			// and we should return with 1 so the keep alive
			// script will exit
			exit (1);
		// port is not in use, return 0
		exit(0);
	}

	// need threads here for tests?

	// gb thrutest <testDir> <fileSize>
	if ( strcmp ( cmd , "thrutest" ) == 0 ) {
		if ( cmdarg+2 >= argc ) goto printHelp;
		char     *testdir         = argv[cmdarg+1];
		int64_t fileSize        = atoll1 ( argv[cmdarg+2] );
		thrutest ( testdir , fileSize );
		return 0;
	}
	// gb seektest <testdir> <numThreads> <maxReadSize>
	if ( strcmp ( cmd , "seektest" ) == 0 ) {
		char     *testdir         = "/tmp/";
		int32_t      numThreads      = 20; //30;
		int64_t maxReadSize     = 20000;
		char     *filename        = NULL;
		if ( cmdarg+1 < argc ) testdir     = argv[cmdarg+1];
		if ( cmdarg+2 < argc ) numThreads  = atol(argv[cmdarg+2]);
		if ( cmdarg+3 < argc ) maxReadSize = atoll1(argv[cmdarg+3]);
		if ( cmdarg+4 < argc ) filename    = argv[cmdarg+4];
		seektest ( testdir , numThreads , maxReadSize , filename );
		return 0;
	}

	/*
	if ( strcmp ( cmd, "qa" ) == 0 ) {
		if ( ! g_hostdb.init(hostsConf, hostId) ) {
			log("db: hostdb init failed." ); return 1; }
		// init our table for doing zobrist hashing
		if ( ! hashinit() ) {
			log("db: Failed to init hashtable." ); return 1; }
		if (!ucInit(g_hostdb.m_dir)) {
			log("Unicode initialization failed!");
			return 1;
		}
		char *s1 = NULL;
		char *s2 = NULL;
		char *u = NULL;
		char *q = NULL;

		if ( cmdarg+1 < argc ) s1 = argv[cmdarg+1];
		if ( cmdarg+2 < argc ) s2 = argv[cmdarg+2];
		if ( cmdarg+3 < argc ) u  = argv[cmdarg+3];
		if ( cmdarg+4 < argc ) q  = argv[cmdarg+4];
		
		qaTest(s1, s2, u, q);
		return 0;
	}
	// gb xmldiff file1 file2
	if (strcmp ( cmd, "xmldiff" )  == 0 ) {
		if ( cmdarg+2 >= argc ) goto printHelp;
		// init our table for doing zobrist hashing
		if ( ! g_hostdb.init(hostsConf, hostId) ) {
			log("db: hostdb init failed." ); return 1; }
		if ( ! hashinit() ) {
			log("db: Failed to init hashtable." ); return 1; }
		if (!ucInit(g_hostdb.m_dir)) {
			log("Unicode initialization failed!");
			return 1;
		}
		DiffOpt opt;
		int nextArg = cmdarg+1;
		while ( argc > nextArg && argv[nextArg][0] == '-'){
			char *c = argv[nextArg] + 1;
			while (*c){
				switch(*c++){
				case 't': opt.m_tagOnly = true; break;
				case 'd': opt.m_debug++       ; break;
				case 'c': opt.m_context++     ; break;
				default: goto printHelp;
				}
			}
			nextArg++;
		}
		if ( nextArg+1 >= argc ) goto printHelp;
		char *file1         = argv[nextArg  ];
		char *file2         = argv[nextArg+1];
		xmlDiffTest(file1, file2, &opt);
		return 0;
	}
	*/

	// note the stack size for debug purposes
	struct rlimit rl;
	getrlimit(RLIMIT_STACK, &rl);
	log(LOG_INFO,"db: Stack size is %"INT64".", (int64_t)rl.rlim_cur);


	// let's ensure our core file can dump
	struct rlimit lim;
	lim.rlim_cur = lim.rlim_max = RLIM_INFINITY;
	if ( setrlimit(RLIMIT_CORE,&lim) ) {
		log("db: setrlimit: %s.", mstrerror(errno) );
	}

	// limit fds
	// try to prevent core from systems where it is above 1024
	// because our FD_ISSET() libc function will core! (it's older)
	int32_t NOFILE = 1024;
	lim.rlim_cur = lim.rlim_max = NOFILE;
	if ( setrlimit(RLIMIT_NOFILE,&lim)) {
		log("db: setrlimit RLIMIT_NOFILE %"INT32": %s.",
		    NOFILE,mstrerror(errno) );
	}

	struct rlimit rlim;
	getrlimit ( RLIMIT_NOFILE,&rlim);
	if ( (int32_t)rlim.rlim_max > NOFILE || (int32_t)rlim.rlim_cur > NOFILE ) {
		log("db: setrlimit RLIMIT_NOFILE failed!");
		char *xx=NULL;*xx=0;
	}

	// set the s_pages array for print admin pages
	g_pages.init ( );

	bool isProxy = false;
	if ( strcmp( cmd , "proxy" ) == 0 && strcmp( argv[cmdarg+1] , "load" ) == 0 ) {
		isProxy = true;
	}

	// this is just like starting up a gb process, but we add one to
	// each port, we are a dummy machine in the dummy cluster.
	// gb -w <workingdir> tmpstart [hostId]
	char useTmpCluster = 0;
	if ( strcmp ( cmd , "tmpstart" ) == 0 ) {
		useTmpCluster = 1;
	}

	// gb -w <workingdir> tmpstop [hostId]
	if ( strcmp ( cmd , "tmpstop" ) == 0 ) {
		useTmpCluster = 1;
	}

	// gb -w <workingdir> tmpstarthost
	if ( strcmp ( cmd , "tmpstarthost" ) == 0 ) {
		useTmpCluster = 1;
	}

	// gb inject <file> <ip:port> [startdocid]
	// gb inject titledb-coll.main.0 <newhosts.conf> [startdocid]
	// gb inject titledb-somedir <newhosts.conf> [startdocid]
	// gb inject titledb-coll.foobar.5 <newhosts.conf> [startdocid]
	if ( strcmp ( cmd , "inject"  ) == 0 ) {
		if ( argc != cmdarg+3 && 
		     argc != cmdarg+4 &&
		     argc != cmdarg+5 ) 
			goto printHelp;
		char *file = argv[cmdarg+1];
		char *ips  = argv[cmdarg+2];
		char *coll = argv[cmdarg+3];
		// int64_t startDocId = 0LL;
		// int64_t endDocId   = DOCID_MASK;
		// if ( cmdarg+3 < argc ) startDocId = atoll(argv[cmdarg+3]);
		// if ( cmdarg+4 < argc ) endDocId   = atoll(argv[cmdarg+4]);
		//injectFile ( file , ips , startDocId , endDocId , false );
		injectFile ( file , ips , coll );
		return 0;
	}

	//
	// get current working dir that the gb binary is in. all the data
	// files should in there too!!
	char *workingDir = getcwd2 ( argv[0] );
	if ( ! workingDir ) {
		fprintf(stderr,"could not get working dir. Exiting.\n");
		return 1;
	}

	//log("host: working directory is %s",workingDir);

	//initialize IP address checks
	initialize_ip_address_checks();
	
	// load up hosts.conf
	// . it will determine our hostid based on the directory path of this
	//   gb binary and the ip address of this server
	if ( ! g_hostdb.init(-1, NULL, isProxy, useTmpCluster, workingDir)) {
		log("db: hostdb init failed." ); return 1;
	}

	Host *h9 = g_hostdb.m_myHost;

	// set clock file name so gettimeofdayInMmiilisecondsGlobal()
	// see g_clockInSync to be true... unles clockadjust.dat is more
	// than 2 days old in which case not!
	if ( g_hostdb.m_myHost->m_hostId != 0 ) {
		// host #0 does not need this, everyone syncs with him
		setTimeAdjustmentFilename(g_hostdb.m_dir , "clockadjust.dat");

		// might as well load it i guess
		loadTimeAdjustment();
	}

	// init our table for doing zobrist hashing
	if ( ! hashinit() ) {
		log("db: Failed to init hashtable." );
		return 1;
	}

	// . hashinit() calls srand() w/ a fixed number
	// . let's mix it up again
	srand ( time(NULL) );

	// do not save conf if any core dump occurs starting here
	// down to where we set this back to true
	g_conf.m_save = false;
	
	//
	// run our smoketests
	//
	/*
	if ( strcmp ( cmd, "qa" ) == 0 ||
	     strcmp ( cmd, "qainject" ) == 0 ||
	     strcmp ( cmd, "qaspider" ) == 0 ) {
		// let's ensure our core file can dump
		struct rlimit lim;
		lim.rlim_cur = lim.rlim_max = RLIM_INFINITY;
		if ( setrlimit(RLIMIT_CORE,&lim) )
			log("qa::setrlimit: %s", mstrerror(errno) );
		// in build mode we store downloaded http replies in the
		// /qa subdir
		//g_conf.m_qaBuildMode = 0;
		//if (  cmdarg+1 < argc )
		//	g_conf.m_qaBuildMode = atoi(argv[cmdarg+1]);
		// 50MB
		g_conf.m_maxMem = 50000000;
		// init our table for doing zobrist hashing
		if ( ! hashinit() ) {
			log("qa::hashinit failed" ); return 0; }
		// init memory class after conf since it gets maxMem from Conf
		if ( ! g_mem.init ( 200000000 ) ) {
			log("qa::Mem init failed" ); return 0; }
		if (!ucInit(g_hostdb.m_dir)) {
			log("Unicode initialization failed!");
			return 1;
		}
		g_conf.m_askRootNameservers = true;
		//g_conf.m_dnsIps  [0]    = atoip ( "192.168.0.1", 11 );
		//g_conf.m_dnsClientPort  = 9909;
		g_conf.m_dnsMaxCacheMem = 1024*10;
		// hack http server port to -1 (none)
		//g_conf.m_httpPort           = 0;
		g_conf.m_httpMaxSockets     = 200;
		//g_conf.m_httpMaxReadBufSize = 102*1024*1024;
		g_conf.m_httpMaxSendBufSize = 16*1024;
		// init the loop
		if ( ! g_loop.init() ) {
			log("qa::Loop init failed" ); return 0; }
		// . then dns client
		// . server should listen to a socket and register with g_loop
		if ( ! g_dns.init(14834)        ) {
			log("qa::Dns client init failed" ); return 0; }
		// . then webserver
		// . server should listen to a socket and register with g_loop
		// . use -1 for both http and https ports to mean do not
		//   listen on any ports. we are a client only.
		if ( ! g_httpServer.init( -1 , -1 ) ) {
			log("qa::HttpServer init failed" ); return 0; }
		// set our new pid
		g_mem.setPid();
		g_threads.setPid();
		g_log.setPid();
		//
		// beging the qaloop
		//
		if ( strcmp(cmd,"qa") == 0 )
			qatest();
		else if ( strcmp(cmd,"qaspider") == 0 )
			qaspider();
		else if ( strcmp(cmd,"qainject") == 0 )
			qainject();

		//
		// wait for some i/o signals
		//
		if ( ! g_loop.runLoop()    ) {
			log("db: runLoop failed." ); 
			return 1; 
		}
		// no error, return 0
		return 0;
	}
	*/


	//Put this here so that now we can log messages
  	if ( strcmp ( cmd , "proxy" ) == 0 ) {
		if (argc < 3){
			goto printHelp;
		}

		int32_t proxyId = -1;
		if ( cmdarg+2 < argc ) proxyId = atoi ( argv[cmdarg+2] );
		
		if ( strcmp ( argv[cmdarg+1] , "start" ) == 0 ) {
			return install ( ifk_proxy_start , proxyId );
		}
		if ( strcmp ( argv[cmdarg+1] , "dstart" ) == 0 ) {
			return install ( ifk_proxy_kstart , proxyId );
		}

		else if ( strcmp ( argv[cmdarg+1] , "stop" ) == 0 ) {
			g_proxy.m_proxyRunning = true;
			return doCmd ( "save=1" , proxyId , "master" ,
				       false,//sendtohosts 
				       true);//sendtoproxies
		}

		else if ( strcmp ( argv[cmdarg+1] , "replacehost" ) == 0 ) {
			g_proxy.m_proxyRunning = true;
			int32_t hostId = -1;
			int32_t spareId = -1;
			if ( cmdarg + 2 < argc ) 
				hostId = atoi ( argv[cmdarg+2] );
			if ( cmdarg + 2 < argc ) 
				spareId = atoi ( argv[cmdarg+3] );
			char replaceCmd[256];
			sprintf(replaceCmd, "replacehost=1&rhost=%"INT32"&rspare=%"INT32"",
				hostId, spareId);
			return doCmd ( replaceCmd, -1, "admin/hosts" ,
				       false,//sendtohosts 
				       true);//sendtoproxies
		}

		else if ( proxyId == -1 || strcmp ( argv[cmdarg+1] , "load" ) != 0 ) {
			goto printHelp;
		}

		Host *h = g_hostdb.getProxy( proxyId );
		uint16_t httpPort = h->m_httpPort;
		uint16_t httpsPort = h->m_httpsPort;
		//we need udpserver for addurl and udpserver2 for pingserver
		uint16_t udpPort  = h->m_port;
		//uint16_t udpPort2 = h->m_port2;
		// g_conf.m_maxMem = 2000000000;

		if ( ! g_conf.init ( h->m_dir ) ) { // , h->m_hostId ) ) {
			log("db: Conf init failed." ); return 1; }

		// init the loop before g_process since g_process
		// registers a sleep callback!
		if ( ! g_loop.init() ) {
			log("db: Loop init failed." ); return 1; }

		//if ( ! g_threads.init()     ) {
		//	log("db: Threads init failed." ); return 1; }

		g_process.init();

		if ( ! g_process.checkNTPD() ) 
			return log("db: ntpd not running on proxy");

		if ( !ucInit(g_hostdb.m_dir))
			return log("db: Unicode initialization failed!");

		// load speller unifiedDict for spider compression proxy
		//if ( g_hostdb.m_myHost->m_type & HT_SCPROXY )
		//	g_speller.init();

		if ( ! g_udpServer.init( g_hostdb.getMyPort() ,
					 &g_dp,
					 20000000 ,   // readBufSIze
					 20000000 ,   // writeBufSize
					 20       ,   // pollTime in ms
					 3500     , // max udp slots
					 false    )){ // is dns?
			log("db: UdpServer init failed." ); return 1; }


		if (!g_proxy.initProxy (proxyId, udpPort, 0, &g_dp))
			return log("proxy: init failed");

		// then statsdb
		if ( ! g_statsdb.init() ) {
			log("db: Statsdb init failed." ); return 1; }

		// init our table for doing zobrist hashing
		if ( ! hashinit() ) {
			log("db: Failed to init hashtable." ); return 1; }

		if ( ! g_proxy.initHttpServer( httpPort, httpsPort ) ) {
			log("db: HttpServer init failed. Another gb "
			    "already running? If not, try editing "
			    "./hosts.conf to "
			    "change the port from %"INT32" to something bigger. "
			    "Or stop gb by running 'gb stop' or by "
			    "clicking 'save & exit' in the master controls."
			    , (int32_t)httpPort ); 
			// this is dangerous!!! do not do the shutdown thing
			return 1;
		}		
		
		//we should save gb.conf right ?
		g_conf.m_save = true;

		if ( ! g_loop.runLoop()    ) {
			log("db: runLoop failed." ); 
			return 1; 
		}

		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}

  	if ( strcmp ( cmd , "blaster" ) == 0 ) {
		int32_t i=cmdarg+1;
		bool isLogFile=false;
		bool injectUrlWithLinks=false;
		bool injectUrl=false;
		int32_t wait = 0;
		
		if ( strcmp (argv[i],"-l") == 0 ){
			isLogFile=true;
			i++;
		}
		if ( strcmp (argv[i],"-i") == 0 ){
			injectUrlWithLinks=true;
			i++;
		}
		if ( strcmp (argv[i],"-u") == 0 ){
			injectUrl=true;
			i++;
		}

		char *filename = argv[i];
		int32_t maxNumThreads=1;
		if (argv[i+1])  maxNumThreads=atoi(argv[i+1]);
		if (argv[i+2]) wait=atoi(argv[i+2]);
		g_conf.m_maxMem = 2000000000;
		//wait atleast 10 msec before you start again.
		if (wait<1000) wait=10;
		g_blaster.runBlaster (filename,NULL,
					      maxNumThreads,wait,
					      isLogFile,false,false,false,
				      injectUrlWithLinks,
				      injectUrl);
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}

	if ( strcmp ( cmd , "blasterdiff" ) == 0 ) {
		int32_t i=cmdarg+1;
		bool verbose=false;
		bool justDisplay=false;
		bool useProxy=true;
		//cycle through the arguments to check for -v,-j,-p
		while (argv[i] && argv[i][0]=='-'){
			if ( strcmp (argv[i],"-v") == 0 ){
				verbose=true;
			}
			else if ( strcmp (argv[i],"-j") == 0 ){
				justDisplay=true;
			}
			else if ( strcmp (argv[i],"-p") == 0){
				useProxy=false;
			}
			i++;
		}

		char *file1 = argv[i];
		char *file2 = argv[i+1];
		int32_t maxNumThreads=1;
		if (argv[i+2])  maxNumThreads=atoi(argv[i+2]);
		int32_t wait;
		if (argv[i+3]) wait=atoi(argv[i+3]);
		//wait atleast 1 sec before you start again.
		if (wait<1000) wait=1000;
		g_blaster.runBlaster(file1,file2,
				     maxNumThreads,wait,false,
				     verbose,justDisplay,useProxy);
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}

	// g_conf.init was here

	// now that we have init'd g_hostdb and g_log, call this for an ssh
	//if ( strcmp ( cmd , "gendbs" ) == 0 && cmdarg + 2 == argc )
	//	return install ( ifk_gendbs , -1 , NULL , 
	//			 argv[cmdarg+1] ); // coll

	if( strcmp(cmd, "distributeC") == 0 && cmdarg +2 == argc ) {
		return install ( ifk_distributeC, -1, NULL, argv[cmdarg+1] );
	}

	//if ( strcmp ( cmd, "genclusterdb" ) == 0 && cmdarg + 2 == argc )
	//	return install ( ifk_genclusterdb , -1 , NULL ,
	//			 argv[cmdarg+1] ); // coll

	// . dumpmissing <coll> [hostid]
	// . if hostid not there, ssh to all using install()
	if ( strcmp ( cmd, "dumpmissing" ) == 0 && cmdarg + 2 == argc ) {
		return install ( ifk_dumpmissing , -1 , NULL , argv[cmdarg+1] ); // coll
	}

	if ( strcmp ( cmd, "dumpdups" ) == 0 && cmdarg + 2 == argc ) {
		return install ( ifk_dumpdups , -1 , NULL , argv[cmdarg+1] ); // coll
	}

	// . gb removedocids <coll> <docIdsFilename> [hostid1-hostid2]
	// . if hostid not there, ssh to all using install()
	// . use removedocids below if only running locally
	// . cmdarg+3 can be 4 or 5, depending if [hostid1-hostid2] is present
	// . argc is 5 if [hostid1-hostid2] is present, 4 if not
	if ( strcmp ( cmd, "removedocids" ) == 0 && cmdarg + 3 >= 4 ) {
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 3 < argc ) hostId = atoi ( argv[cmdarg+3] );
		// might have a range
		if ( cmdarg + 3 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+3],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return install ( ifk_removedocids , 
						 h1, 
						 argv[cmdarg+2], // filename
						 argv[cmdarg+1], // coll
						 h2            );
		}
		// if we had no hostid given, cast to all
		if ( hostId == -1 )
			return install ( ifk_removedocids , 
					 -1            ,  // hostid1
					 argv[cmdarg+2], // filename
					 argv[cmdarg+1], // coll
					 -1            ); // hostid2
		// otherwise, a hostid was given and we will call
		// removedocids() directly below
	}

	// gb ping [hostId] [clientPort]
	if ( strcmp ( cmd , "ping" ) == 0 ) {
		int32_t hostId = 0;
		if ( cmdarg + 1 < argc ) {
			hostId = atoi ( argv[cmdarg+1] );
		}

		uint16_t port = 2050;
		if ( cmdarg + 2 < argc ) {
			port = (uint16_t)atoi ( argv[cmdarg+2] );
		}

		pingTest ( hostId , port );

		return 0;
	}

	// gb injecttest <requestLen> [hostId]
	if ( strcmp ( cmd , "injecttest" ) == 0 ) {
		if ( cmdarg+1 >= argc ) {
			goto printHelp;
		}

		int32_t hostId = 0;
		if ( cmdarg + 2 < argc ) {
			hostId = atoi ( argv[cmdarg+2] );
		}

		int32_t reqLen = atoi ( argv[cmdarg+1] );
		if ( reqLen == 0 ) {
			goto printHelp;
		}

		injectFileTest ( reqLen , hostId );
		return 0;
	}

	/*
	// gb inject <file> <ip:port> [startdocid]
	// gb inject titledb <newhosts.conf> [startdocid]
	if ( strcmp ( cmd , "inject"  ) == 0 ) {
		if ( argc != cmdarg+3 && 
		     argc != cmdarg+4 &&
		     argc != cmdarg+5 ) 
			goto printHelp;
		char *file = argv[cmdarg+1];
		char *ips  = argv[cmdarg+2];
		int64_t startDocId = 0LL;
		int64_t endDocId   = DOCID_MASK;
		if ( cmdarg+3 < argc ) startDocId = atoll(argv[cmdarg+3]);
		if ( cmdarg+4 < argc ) endDocId   = atoll(argv[cmdarg+4]);
		injectFile ( file , ips , startDocId , endDocId , false );
		return 0;
	}
	*/
	/*
	if ( strcmp ( cmd , "reject"  ) == 0 ) {
		if ( argc != cmdarg+3 && 
		     argc != cmdarg+4 &&
		     argc != cmdarg+5 ) 
			goto printHelp;
		char *file = argv[cmdarg+1];
		char *ips  = argv[cmdarg+2];
		int64_t startDocId = 0LL;
		int64_t endDocId   = DOCID_MASK;
		//if ( cmdarg+3 < argc ) startDocId = atoll(argv[cmdarg+3]);
		//if ( cmdarg+4 < argc ) endDocId   = atoll(argv[cmdarg+4]);
		injectFile ( file , ips , startDocId , endDocId , true );
		return 0;
	}
	*/

	// gb dsh
	if ( strcmp ( cmd , "dsh" ) == 0 ) {	
		if ( cmdarg+1 >= argc ) {
			goto printHelp;
		}

		char *cmd = argv[cmdarg+1];
		return install ( ifk_dsh , -1, NULL, NULL, -1, cmd );
	}

	// gb dsh2
	if ( strcmp ( cmd , "dsh2" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		//int32_t hostId = -1;
		if ( cmdarg+1 >= argc ) goto printHelp;
		char *cmd = argv[cmdarg+1];
		return install ( ifk_dsh2 , -1,NULL,NULL,-1, cmd );
	}

	// gb copyfiles, like gb install but takes a dir not a host #
	if ( strcmp ( cmd , "copyfiles" ) == 0 ) {	
		if ( cmdarg + 1 >= argc ) goto printHelp;
		char *dir = argv[cmdarg+1];
		return copyFiles ( dir );
	}

	// gb install
	if ( strcmp ( cmd , "install" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t h1 = -1;
		int32_t h2 = -1;
		if ( cmdarg + 1 < argc ) h1 = atoi ( argv[cmdarg+1] );
		// might have a range
		if (cmdarg + 1 < argc && strstr(argv[cmdarg+1],"-") )
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
		return install ( ifk_install , h1 , NULL , NULL , h2 );
	}

	// gb installgb
	if ( strcmp ( cmd , "installgb" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return install ( ifk_installgb , hostId );
	}

	// gb installgbrcp
	if ( strcmp ( cmd , "installgbrcp" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return install ( ifk_installgbrcp , hostId );
	}

	// gb installfile
	if ( strcmp ( cmd , "installfile" ) == 0 ) {	
		if(cmdarg+1 < argc)
			return install_file ( argv[cmdarg+1] );
	}

	// gb installgb
	if ( strcmp ( cmd , "installgb2" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return install ( ifk_installgb2 , hostId );
	}

	// gb installtmpgb
	if ( strcmp ( cmd , "installtmpgb" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return install ( ifk_installtmpgb , hostId );
	}

	// gb installconf
	if ( strcmp ( cmd , "installconf" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return install ( ifk_installconf , hostId );
	}

	// gb installconf2
	if ( strcmp ( cmd , "installconf2" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return install ( ifk_installconf2 , hostId );
	}

	// gb start [hostId]
	if ( strcmp ( cmd , "start" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				//
				// default to keepalive start for now!!
				//
				return install ( ifk_kstart , h1, 
						 NULL,NULL,h2 );
		}

		// default to keepalive start for now!! (was ifk_start)
		return install ( ifk_kstart , hostId );
	}

	// gb astart [hostId] (non-keepalive start)
	if ( strcmp ( cmd , "nstart" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return install ( ifk_start , h1, 
						 NULL,NULL,h2 );
		}
		// if it is us, do it
		//if ( hostId != -1 ) goto mainStart;
		return install ( ifk_start , hostId );
	}

	// gb tmpstart [hostId]
	if ( strcmp ( cmd , "tmpstart" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return install ( ifk_tmpstart , h1, 
						 NULL,NULL,h2 );
		}
		// if it is us, do it
		//if ( hostId != -1 ) goto mainStart;
		return install ( ifk_tmpstart, hostId );
	}

	if ( strcmp ( cmd , "tmpstop" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "save=1" , h1 , "master" , 
					       true , //sendtohosts
					       false,//sendtoproxies
					       h2 );
		}
		return doCmd ( "save=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb start2 [hostId]
	if ( strcmp ( cmd , "start2" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return install ( ifk_start2 , h1, 
						 NULL,NULL,h2 );
		}
		// if it is us, do it
		//if ( hostId != -1 ) goto mainStart;
		return install ( ifk_start2 , hostId );
	}

	//keep alive start... not!
	if ( strcmp ( cmd , "dstart" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return install ( ifk_dstart , h1, 
						 NULL,NULL,h2 );
		}
		return install ( ifk_dstart , hostId );
	}

	if ( strcmp ( cmd , "kstop" ) == 0 ) {	
		//same as stop, here for consistency
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "save=1" , h1 , "master" , 
					       true , //sendtohosts
					       false,//sendtoproxies
					       h2 );
		}
		return doCmd ( "save=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies

	}

	// gb backupcopy [hostId] <backupSubdirName>
	if ( strcmp ( cmd , "backupcopy" ) == 0 ) {	
		if ( cmdarg + 1 >= argc ) goto printHelp;
		return install ( ifk_backupcopy , -1 , argv[cmdarg+1] );
	}

	// gb backupmove [hostId] <backupSubdirName>
	if ( strcmp ( cmd , "backupmove" ) == 0 ) {	
		if ( cmdarg + 1 >= argc ) goto printHelp;
		return install ( ifk_backupmove , -1 , argv[cmdarg+1] );
	}

	// gb backupmove [hostId] <backupSubdirName>
	if ( strcmp ( cmd , "backuprestore" ) == 0 ) {	
		if ( cmdarg + 1 >= argc ) goto printHelp;
		return install ( ifk_backuprestore, -1 , argv[cmdarg+1] );
	}

	// gb scale <hosts.conf>
	if ( strcmp ( cmd , "scale" ) == 0 ) {	
		if ( cmdarg + 1 >= argc ) goto printHelp;
		return scale ( argv[cmdarg+1] , true );
	}

	// gb collinject
	if ( strcmp ( cmd , "collinject" ) == 0 ) {	
		if ( cmdarg + 1 >= argc ) goto printHelp;
		return collinject ( argv[cmdarg+1] );
	}

	// gb collcopy <hosts.conf> <coll> <collnum>>
	if ( strcmp ( cmd , "collcopy" ) == 0 ) {	
		if ( cmdarg + 4 != argc ) goto printHelp;
		char *hostsconf = argv[cmdarg+1];
		char *coll      = argv[cmdarg+2];
		int32_t  collnum   = atoi(argv[cmdarg+3]);
		return collcopy ( hostsconf , coll , collnum );
	}

	// gb stop [hostId]
	if ( strcmp ( cmd , "stop" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "save=1" , h1 , "master" , 
					       true , //sendtohosts
					       false,//sendtoproxies
					       h2 );
		}
		return doCmd ( "save=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb save [hostId]
	if ( strcmp ( cmd , "save" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "js=1" , h1 , "master" , 
					       true , //sendtohosts
					       false,//sendtoproxies
					       h2 );
		}
		return doCmd ( "js=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb spidersoff [hostId]
	if ( strcmp ( cmd , "spidersoff" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return doCmd ( "se=0" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb spiderson [hostid]
	if ( strcmp ( cmd , "spiderson" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return doCmd ( "se=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb cacheoff [hostId]
	if ( strcmp ( cmd , "cacheoff" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return doCmd ( "dpco=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb freecache [hostId]
	if ( strcmp ( cmd , "freecache" ) == 0 ) {	
		int32_t max = 7000000;
		if ( cmdarg + 1 < argc ) max = atoi ( argv[cmdarg+1] );
		//freeAllSharedMem( max );
		return true;
	}

	// gb ddump [hostId]
	if ( strcmp ( cmd , "ddump" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		return doCmd ( "dump=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb pmerge [hostId]
	if ( strcmp ( cmd , "pmerge" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "pmerge=1",h1,"master",
					       true , //sendtohosts
					       false ,//sendtoproxiesh2
					       h2 );
		}
		return doCmd ( "pmerge=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb smerge [hostId]
	if ( strcmp ( cmd , "smerge" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "smerge=1",h1,"master",
					       true , //sendtohosts
					       false ,//sendtoproxies
					       h2 );
		}
		return doCmd ( "smerge=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb tmerge [hostId]
	if ( strcmp ( cmd , "tmerge" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "tmerge=1",h1,"master",
					       true , //sendtohosts
					       false, //sendtoproxies
					       h2);
		}
		return doCmd ( "tmerge=1" , hostId , "master" , 
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb merge [hostId]
	if ( strcmp ( cmd , "merge" ) == 0 ) {	
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		// might have a range
		if ( cmdarg + 1 < argc ) {
			int32_t h1 = -1;
			int32_t h2 = -1;
			sscanf ( argv[cmdarg+1],"%"INT32"-%"INT32"",&h1,&h2);
			if ( h1 != -1 && h2 != -1 && h1 <= h2 )
				return doCmd ( "merge=1",h1,"master",
					       true , //sendtohosts
					       false,//sendtoproxies
					       h2);
		}
		return doCmd ( "merge=1" , hostId , "master" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb setnote <hostid> <note>
	if ( strcmp ( cmd, "setnote" ) == 0 ) {
		int32_t hostId;
		char *note;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		else return false;
		if ( cmdarg + 2 < argc ) note = argv[cmdarg+2];
		else return false;
		char urlnote[1024];
		urlEncode(urlnote, 1024, note, gbstrlen(note));
		log ( LOG_INIT, "conf: setnote %"INT32": %s", hostId, urlnote );
		char setnoteCmd[256];
		sprintf(setnoteCmd, "setnote=1&host=%"INT32"&note=%s",
				    hostId, urlnote);
		return doCmd ( setnoteCmd, -1, "admin/hosts" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb setsparenote <spareid> <note>
	if ( strcmp ( cmd, "setsparenote" ) == 0 ) {
		int32_t spareId;
		char *note;
		if ( cmdarg + 1 < argc ) spareId = atoi ( argv[cmdarg+1] );
		else return false;
		if ( cmdarg + 2 < argc ) note = argv[cmdarg+2];
		else return false;
		char urlnote[1024];
		urlEncode(urlnote, 1024, note, gbstrlen(note));
		log(LOG_INIT, "conf: setsparenote %"INT32": %s", spareId, urlnote);
		char setnoteCmd[256];
		sprintf(setnoteCmd, "setsparenote=1&spare=%"INT32"&note=%s",
				    spareId, urlnote);
		return doCmd ( setnoteCmd, -1, "admin/hosts" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}

	// gb replacehost <hostid> <spareid>
	if ( strcmp ( cmd, "replacehost" ) == 0 ) {
		int32_t hostId = -1;
		int32_t spareId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		if ( cmdarg + 2 < argc ) spareId = atoi ( argv[cmdarg+2] );
		char replaceCmd[256];
		sprintf(replaceCmd, "replacehost=1&rhost=%"INT32"&rspare=%"INT32"",
				    hostId, spareId);
		return doCmd ( replaceCmd, -1, "admin/hosts" ,
			       true , //sendtohosts
			       true );//sendtoproxies
	}

	// gb synchost <hostid>
	if ( strcmp ( cmd, "synchost" ) == 0 ) {
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		else return false;
		char syncCmd[256];
		sprintf(syncCmd, "synchost=1&shost=%"INT32"", hostId);
		return doCmd ( syncCmd, g_hostdb.m_hostId, "admin/hosts" ,
			       true , //sendtohosts
			       false );//sendtoproxies
	}
	if ( strcmp ( cmd, "synchost2" ) == 0 ) {
		int32_t hostId = -1;
		if ( cmdarg + 1 < argc ) hostId = atoi ( argv[cmdarg+1] );
		else return false;
		char syncCmd[256];
		sprintf(syncCmd, "synchost=2&shost=%"INT32"", hostId);
		return doCmd ( syncCmd, g_hostdb.m_hostId, "admin/hosts" ,
		true, //sendToHosts
		false );// sendtoproxies
	}

	// gb [-h hostsConf] <hid>
	// mainStart:

	// get host info for this host
	//Host *h = g_hostdb.getHost ( hostId );
	//if ( ! h ) { log("db: No host has id %"INT32".",hostId); return 1;}

	// once we are in recoverymode, that means we are being restarted
	// from having cored, so to prevent immediate core and restart
	// ad inifinitum, look got "sigbadhandler" at the end of the 
	// last 5 logs in the last 60 seconds. if we see that then something
	// is prevent is from starting up so give up and exit gracefully
	if ( g_recoveryMode && isRecoveryFutile () ) {
		// exiting with 0 means no error and should tell our
		// keep alive loop to not restart us and exit himself.
		exit (0);
	}


	// HACK: enable logging for Conf.cpp, etc.
	g_process.m_powerIsOn = true;

	// . read in the conf file
	// . this now initializes from a dir and hostId, they should all be
	//   name gbHID.conf
	// . now that hosts.conf has more of the burden, all gbHID.conf files
	//   can be identical
	if ( ! g_conf.init ( h9->m_dir ) ) {
		log("db: Conf init failed." );
		return 1;
	}

	//if ( ! g_hostdb.validateIps ( &g_conf ) ) {
	//	log("db: Failed to validate ips." ); return 1;}
	//if ( ! g_hostdb2.validateIps ( &g_conf ) ) {
	//	log("db: Failed to validate ips." ); return 1;}

	// put in read only mode
	if ( useTmpCluster ) {
		g_conf.m_readOnlyMode = true;
		g_conf.m_sendEmailAlerts = false;
	}

	// log how much mem we can use
	//log(LOG_INIT,"conf: Max mem allowed to use is %"INT64"\n",
	//g_conf.m_maxMem);

	// init the loop, needs g_conf
	if ( ! g_loop.init() ) {
		log("db: Loop init failed." );
		return 1;
	}

	// the new way to save all rdbs and conf
	// if g_process.m_powerIsOn is false, logging will not work, so init
	// this up here. must call after Loop::init() so it can register
	// its sleep callback
	g_process.init();

	// set up the threads, might need g_conf

	// avoid logging threads msgs to stderr if not actually starting up
	// a gb daemon...
	//if(cmd && cmd[0] && ! is_digit(cmd[0]) && ! g_threads.init()     ) {
	//if ( ! g_threads.init()     ) {
	//	log("db: Threads init failed." ); return 1; }

	// gb gendict
	if ( strcmp ( cmd , "gendict" ) == 0 ) {	
		// get hostId to install TO (-1 means all)
		if ( argc != cmdarg + 2 &&
		     argc != cmdarg + 3 ) goto printHelp; // take no other args
		char *coll = argv[cmdarg+1];
		// get numWordsToDump
		int32_t  nn = 10000000;
		if ( argc == cmdarg + 3 ) nn = atoi ( argv[cmdarg+2] );
		// . generate the dict files
		// . use the first 100,000,000 words/phrases to make them
		g_speller.generateDicts ( nn , coll );
		return 0;
	}
	if ( strcmp ( cmd , "dumpmissing" ) == 0 ) {	
		// got collection and hostid in here
		if ( argc != cmdarg + 3 ) goto printHelp;
		char *coll = argv[cmdarg+1];
		dumpMissing ( coll );
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}
	if ( strcmp ( cmd , "dumpdups" ) == 0 ) {	
		// got collection and hostid in here
		if ( argc != cmdarg + 3 ) goto printHelp;
		char *coll = argv[cmdarg+1];
		dumpDups ( coll );
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}
	// removedocids <coll> <filename> <hostid>
	if ( strcmp ( cmd , "removedocids" ) == 0 ) {	
		if ( argc != cmdarg + 4 ) goto printHelp;
		char *coll = argv[cmdarg+1];
		char *file = argv[cmdarg+2];
		removeDocIds ( coll , file );
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}


#ifndef _CLIENT_
	// gb dump i [fileNum] [off]
	if ( strcmp ( cmd , "dump" ) == 0 && argc > cmdarg + 1 &&
	     argv[cmdarg+1][0]=='I')  {		

		int32_t      fileNum = 0;
		int64_t off     = 0LL;
		char     *NAME = NULL;
		//if ( cmdarg + 2 < argc ) fileNum = atoi  (argv[cmdarg+2]);
		if ( cmdarg + 2 < argc ) NAME = argv[cmdarg+2];
		if ( cmdarg + 3 < argc ) off  = atoll1(argv[cmdarg+3]);
		dumpIndexdbFile ( fileNum , off , "indexdb" , 12 , NAME );
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}

	if ( strcmp ( cmd , "rmtest" ) == 0 ) {
		rmTest();
		return 0;
	}

	if ( strcmp ( cmd , "dump" ) == 0 && argc > cmdarg + 1 &&
	     argv[cmdarg+1][0]=='T')  {

		int32_t      fileNum = 0;
		int64_t off     = 0LL;
		if ( cmdarg + 2 < argc ) fileNum = atoi  (argv[cmdarg+2]);
		if ( cmdarg + 3 < argc ) off     = atoll1(argv[cmdarg+3]);
		dumpIndexdbFile ( fileNum , off , "datedb" , 16 );
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}
#endif

	// . gb dump [dbLetter][coll][fileNum] [numFiles] [includeTree][termId]
	// . spiderdb is special:
	//   gb dump s [coll][fileNum] [numFiles] [includeTree] [0=old|1=new]
	//           [priority] [printStats?]
	if ( strcmp ( cmd , "dump" ) == 0 ) {

		// if ( ! hadHostId ) {
		// 	log("you must supply hostid in the dump cmd");
		// 	return 0;
		// }

		//
		// tell Collectiondb, not to verify each rdb's data
		//
		g_dumpMode = true;

		if ( cmdarg+1 >= argc ) goto printHelp;
		int32_t startFileNum =  0;
		int32_t numFiles     = -1;
		int32_t includeTree  =  1;
		int64_t termId  = -1;
		char *coll = "";

		// so we do not log every collection coll.conf we load
		g_conf.m_doingCommandLine = true;

		// we have to init collection db because we need to know if 
		// the collnum is legit or not in the tree
		if ( ! g_collectiondb.loadAllCollRecs()   ) {
			log("db: Collectiondb init failed." ); return 1; }

		if ( cmdarg+2 < argc ) coll         = argv[cmdarg+2];
		if ( cmdarg+3 < argc ) startFileNum = atoi(argv[cmdarg+3]);
		if ( cmdarg+4 < argc ) numFiles     = atoi(argv[cmdarg+4]);
		if ( cmdarg+5 < argc ) includeTree  = atoi(argv[cmdarg+5]);
		if ( cmdarg+6 < argc ) {
			char *targ = argv[cmdarg+6];
			if ( is_alpha_a(targ[0]) ) {
				char *colon = strstr(targ,":");
				int64_t prefix64 = 0LL;
				if ( colon ) {
					*colon = '\0';
					prefix64 = hash64n(targ);
					targ = colon + 1;
				}
				// hash the term itself
				termId = hash64n(targ);
				// hash prefix with termhash
				if ( prefix64 )
					termId = hash64(termId,prefix64);
				termId &= TERMID_MASK;
			}
			else {
				termId = atoll1(targ);
			}
		}
		if ( argv[cmdarg+1][0] == 't' ) {
			int64_t docId = 0LL;
			if ( cmdarg+6 < argc ) {
				docId = atoll1(argv[cmdarg+6]);
			}

			dumpTitledb (coll, startFileNum, numFiles, includeTree, docId, false);

		}
		else if ( argv[cmdarg+1][0] == 'D' ) {
			int64_t docId = 0LL;
			if ( cmdarg+6 < argc ) {
				docId = atoll1(argv[cmdarg+6]);
			}

			dumpTitledb (coll, startFileNum, numFiles, includeTree, docId, true);
		}
		else if ( argv[cmdarg+1][0] == 'w' )
		       dumpWaitingTree(coll);
		else if ( argv[cmdarg+1][0] == 'x' )
			dumpDoledb  (coll,startFileNum,numFiles,includeTree);
		else if ( argv[cmdarg+1][0] == 's' ) {
			char  printStats = 0;
			int32_t firstIp = 0;
			if ( cmdarg+6 < argc ){
				printStats= atol(argv[cmdarg+6]);
				// it could be an ip instead of printstats
				if ( strstr(argv[cmdarg+6],".") ) {
					printStats = 0;
					firstIp = atoip(argv[cmdarg+6]);
				}
			}

			int32_t ret = dumpSpiderdb ( coll, startFileNum, numFiles, includeTree, printStats, firstIp );
			if ( ret == -1 ) {
				fprintf(stdout,"error dumping spiderdb\n");
			}
		}
		else if ( argv[cmdarg+1][0] == 'S' ) {
			char *site = NULL;
			if ( cmdarg+6 < argc ) {
				site = argv[cmdarg+6];
			}
			dumpTagdb( coll, startFileNum, numFiles, includeTree, 0, RDB_TAGDB, site );
		} else if ( argv[cmdarg+1][0] == 'z' ) {
			char *site = NULL;
			if ( cmdarg+6 < argc ) site = argv[cmdarg+6];
			dumpTagdb( coll, startFileNum, numFiles, includeTree, 'z', RDB_TAGDB, site );
		} else if ( argv[cmdarg+1][0] == 'A' ) {
			dumpTagdb( coll, startFileNum, numFiles, includeTree, 'A' );
		} else if ( argv[cmdarg+1][0] == 'G' ) {
			dumpTagdb( coll, startFileNum, numFiles, includeTree, 'G' );
		} else if ( argv[cmdarg+1][0] == 'W' ) {
			dumpTagdb( coll, startFileNum, numFiles, includeTree );
		} else if ( argv[cmdarg+1][0] == 'C' ) {
			dumpTagdb( coll, startFileNum, numFiles, includeTree, 0, RDB_CATDB );
		} else if ( argv[cmdarg+1][0] == 'l' )
			dumpClusterdb (coll,startFileNum,numFiles,includeTree);
		//else if ( argv[cmdarg+1][0] == 'z' )
		//	dumpStatsdb(startFileNum,numFiles,includeTree,2);
		//else if ( argv[cmdarg+1][0] == 'Z' )
		//	dumpStatsdb(startFileNum,numFiles,includeTree,4);
		else if ( argv[cmdarg+1][0] == 'L' ) {
			char *url = NULL;
			if ( cmdarg+6 < argc ) url = argv[cmdarg+6];
			dumpLinkdb(coll,startFileNum,numFiles,includeTree,url);
		}
#ifndef _CLIENT_
		else if ( argv[cmdarg+1][0] == 'i' )
			dumpIndexdb (coll,startFileNum,numFiles,includeTree,
				     termId);
		else if ( argv[cmdarg+1][0] == 'p' )
			dumpPosdb (coll,startFileNum,numFiles,includeTree,
				     termId,false);
		else if ( argv[cmdarg+1][0] == 'd' )
			dumpDatedb  (coll,startFileNum,numFiles,includeTree,
				     termId,false);
#endif
		/*
		else if      ( argv[cmdarg+1][0] == 'R' ) {
			int64_t docId = 0LL;
			if ( cmdarg+6 < argc ) docId = atoll1(argv[cmdarg+6]);
			testSpamRules (coll,startFileNum,numFiles,includeTree,
				       docId);
		}
		*/



		else goto printHelp;
		// disable any further logging so final log msg is clear
		g_log.m_disabled = true;
		return 0;
	}

	if( strcmp( cmd, "countdomains" ) == 0 && argc >= (cmdarg + 2) ) {
		char *coll = "";
		int32_t verb;
		int32_t outpt;
		coll = argv[cmdarg+1];
		if( argv[cmdarg+2][0] < 0x30 && argv[cmdarg+2][0] > 0x39 )
			goto printHelp;
		int32_t numRecs = atoi( argv[cmdarg+2] );

		if( argc > (cmdarg + 2) ) verb = atoi( argv[cmdarg+2] );
		else verb = 0;

		if( argc > (cmdarg + 3) ) outpt = atoi( argv[cmdarg+3] );
		else outpt = 0;

		log( LOG_INFO, "cntDm: Allocated Larger Mem Table for: %"INT32"",
		     g_mem.m_memtablesize );
		if (!ucInit(g_hostdb.m_dir)) {
			log("Unicode initialization failed!");
			return 1;
		}

		countdomains( coll, numRecs, verb, outpt );
		g_log.m_disabled = true;
		return 0;
	}

	// start up log file
	if ( ! g_log.init( g_hostdb.m_logFilename ) ) {
		fprintf (stderr,"db: Log file init failed. Exiting.\n" );
		return 1;
	}

	g_log.m_logTimestamps = true;
	g_log.m_logReadableTimestamps = true;	// @todo BR: Should be configurable..

	// temp merge test
	//RdbList list;
	//list.testIndexMerge();

	// file creation test, make sure we have dir control
	if ( checkDirPerms ( g_hostdb.m_dir ) < 0 ) return 1;

	// . make sure we have critical files
	if ( ! g_process.checkFiles ( g_hostdb.m_dir ) ) return 1;

	// load the appropriate dictionaries
	//g_speller.init();
	//if ( !g_speller.init ( ) ) return 1;

	g_errno = 0;

	//g_speller.test ( );
	//exit(-1);
	/*
	char dst[1024];
	char test[1024];
 spellLoop:
	test[0] = '\0';
	gets ( test );
	if ( test[gbstrlen(test)-1] == '\n' ) test[gbstrlen(test)-1] = '\0';
	Query qq;
	qq.set ( test , gbstrlen(test) , NULL , 0 , false );
	if ( g_speller.getRecommendation ( &qq , dst , 1000 ) )
		log("spelling suggestion: %s", dst );
	goto spellLoop;
	*/

	// make sure port is available, no use loading everything up then
	// failing because another process is already running using this port
	if ( ! g_httpServer.m_tcp.testBind(g_hostdb.getMyHost()->m_httpPort, true)) {
		return 1;
	}

	int32_t *ips;

	//if ( strcmp ( cmd , "gendbs"       ) == 0 ) goto jump;
	if ( strcmp ( cmd , "gencatdb"     ) == 0 ) goto jump;
	//if ( strcmp ( cmd , "genclusterdb" ) == 0 ) goto jump;
	//	if ( cmd && ! is_digit(cmd[0]) ) goto printHelp;


	log("db: Logging to file %s.",
	    g_hostdb.m_logFilename );

	if ( ! g_conf.m_runAsDaemon )
		log("db: Use 'gb -d' to run as daemon. Example: "
		    "gb -d");

	/*
	// tmp stuff to generate new query log
	if ( ! ucInit(g_hostdb.m_dir, true)) return 1;
	if ( ! g_wiktionary.load() ) return 1;
	if ( ! g_wiktionary.test() ) return 1;
	if ( ! g_wiki.load() ) return 1;
	if ( ! g_speller.init() && g_conf.m_isLive ) return 1;
	return 0;
	*/


	// in case we do not have one, we need it for Images.cpp
	if ( ! makeTrashDir() ) {
		fprintf (stderr,"db: failed to make trash dir. Exiting.\n" ); 
		return 1; 
	}
		

	g_errno = 0;

	// 
	// run as daemon now
	//
	//fprintf(stderr,"running as daemon\n");
	if ( g_conf.m_runAsDaemon ) {
		pid_t pid, sid;
		pid = fork();
		if ( pid < 0 ) exit(EXIT_FAILURE);
		// seems like we core unless parent sets this to NULL.
		// it does not affect the child.
		//if ( pid > 0 ) g_hostdb.m_myHost = NULL;
		// child gets a 0, parent gets the child's pid, so exit
		if ( pid > 0 ) exit(EXIT_SUCCESS);
		// change file mode mask
		umask(0);
		sid = setsid();
		if ( sid < 0 ) exit(EXIT_FAILURE);
		//fprintf(stderr,"done\n");
		// set our new pid
		g_threads.setPid();

		// if we do not do this we don't get sigalarms or quickpolls
		// when running as 'gb -d'
		g_loop.init();
	}

	// initialize threads down here now so it logs to the logfile and
	// not stderr
	//if ( ( ! cmd || !cmd[0]) && ! g_threads.init()     ) {
	//	log("db: Threads init failed." ); return 1; }

	// log the version
	log(LOG_INIT,"conf: Gigablast Version: %s",getVersion());
	log(LOG_INIT,"conf: Gigablast Architecture: %"INT32"-bit\n",arch);


	// show current working dir
	log("host: Working directory is %s",workingDir);

	log("host: Using %shosts.conf",g_hostdb.m_dir);

	{
		pid_t pid = getpid();
		log("host: Process ID is %"UINT64"",(int64_t)pid);
	}

	// from Hostdb.cpp
	ips = getLocalIps();
	for ( ; ips && *ips ; ips++ )
		log("host: Detected local ip %s",iptoa(*ips));

	// show it
	log("host: Running as host id #%"INT32"",g_hostdb.m_hostId );


	if (!ucInit(g_hostdb.m_dir)) {
		log("Unicode initialization failed!");
		return 1;
	}

	// some tests. the greek letter alpha with an accent mark (decompose)
	/*
	{
		char us[] = {0xe1,0xbe,0x80};
		UChar32 uc = utf8Decode(us);//,&next);
		UChar32 ttt[32];
		int32_t klen = recursiveKDExpand(uc,ttt,256);
		char obuf[64];
		for ( int32_t i = 0 ; i < klen ; i++ ) {
			UChar32 ui = ttt[i];
			int32_t blen = utf8Encode(ui,obuf);
			obuf[blen]=0;
			int32_t an = ucIsAlpha(ui);
			
			fprintf(stderr,"#%"INT32"=%s (alnum=%"INT32")\n",i,obuf,an);
		}
		fprintf(stderr,"hey\n");
		exit(0);
	}
	*/

	/*

	  PRINT OUT all Unicode characters and their decompositions

	{
		for ( int32_t uc = 0 ; uc < 0xe01ef ; uc++ ) {
			//if ( ! ucIsAlnum(uc) ) continue;
			UChar32 ttt[32];
			int32_t klen = recursiveKDExpand(uc,ttt,256);
			char obuf[64];
			int32_t clen = utf8Encode(uc,obuf);
			obuf[clen]=0;
			// print utf8 char we are decomposing
			fprintf(stderr,"%"XINT32") %s --> ",uc,obuf);
			// sanity
			if ( klen > 1 && ttt[0] == (UChar32)uc ) {
				fprintf(stderr,"SAME\n");
				continue;
			}
			// print decomposition
			for ( int32_t i = 0 ; i < klen ; i++ ) {
				UChar32 ui = ttt[i];
				char qbuf[64];
				int32_t blen = utf8Encode(ui,qbuf);
				qbuf[blen]=0;
				fprintf(stderr,"%s",qbuf);
				// show the #
				fprintf(stderr,"{%"XINT32"}",(int32_t)ui);
				if ( i+1<klen ) fprintf(stderr,", ");
			}
			// show utf8 rep
			fprintf(stderr," [");
			for ( int32_t i = 0 ; i < clen ; i++ ) {
				fprintf(stderr,"0x%hhx",(int)obuf[i]);
				if ( i+1<clen) fprintf(stderr," ");
			}
			fprintf(stderr,"]");
			fprintf(stderr,"\n");
		}
		exit(0);
	}
	*/			


	

	// the wiktionary for lang identification and alternate word forms/
	// synonyms
	if ( ! g_wiktionary.load() ) return 1;
	if ( ! g_wiktionary.test() ) return 1;

	// . load synonyms, synonym affinity, and stems
	// . now we are using g_synonyms
	//g_thesaurus.init();
	//g_synonyms.init();

	// the wiki titles
	if ( ! g_wiki.load() ) return 1;

 jump:
	// force give up on dead hosts to false
	g_conf.m_giveupOnDeadHosts = 0;

	// shout out if we're in read only mode
	if ( g_conf.m_readOnlyMode )
		log("db: -- Read Only Mode Set. Can Not Add New Data. --");
//#ifdef SPLIT_INDEXDB
	//if ( g_hostdb.m_indexSplits > 1 )
	//	log("db: -- Split Index ENABLED. Split count set to: %"INT32" --",
	//	    g_hostdb.m_indexSplits);
//#endif

	// the robots.txt db
	//if ( ! g_robotdb.init() ) {
	//	log("db: Robotdb init failed." ); return 1; }

	// . collectiondb, does not use rdb, loads directly from disk
	// . do this up here so RdbTree::fixTree() can fix RdbTree::m_collnums
	// . this is a fake init, cuz we pass in "true"
	if ( ! g_collectiondb.loadAllCollRecs() ) {
		log("db: Collectiondb load failed." ); return 1; }

	// a hack to rename files that were not renamed because of a bug
	// in the repair/build process
	/*
	if ( ! g_titledb2.init2    ( 100000000 ) ) {
		log("db: Titledb init2 failed." ); return 1; }
	if ( ! g_titledb2.addRdbBase1  ( "mainRebuild" ) ) {
		log("db: Titledb addcoll failed." ); return 1; }
	g_titledb2
	// get the base
	RdbBase *base = g_titledb2.m_rdb.m_bases[1];
	// panic?
	if ( ! base ) { log("db: titledb2: no base."); return 1; }
	// now clean them up
	base->removeRebuildFromFilenames ( ) ;
	// stop
	return 1;
	*/

	// then statsdb
	if ( ! g_statsdb.init() ) {
		log("db: Statsdb init failed." ); return 1; }

	// allow adds to statsdb rdb tree
	g_process.m_powerIsOn = true;

	//	log("db: Indexdb init failed." ); return 1; }
	if ( ! g_posdb.init()    ) {
		log("db: Posdb init failed." ); return 1; }
	// then titledb
	if ( ! g_titledb.init()    ) {
		log("db: Titledb init failed." ); return 1; }
	// then tagdb
	if ( ! g_tagdb.init()     ) {
		log("db: Tagdb init failed." ); return 1; }

	// then spiderdb
	if ( ! g_spiderdb.init()   ) {
		log("db: Spiderdb init failed." ); return 1; }
	// then doledb
	if ( ! g_doledb.init()   ) {
		log("db: Doledb init failed." ); return 1; }
	// the spider cache used by SpiderLoop
	if ( ! g_spiderCache.init() ) {
		log("db: SpiderCache init failed." ); return 1; }
	if ( ! g_test.init() ) {
		log("db: test init failed" ); return 1; }

	// ensure clusterdb tree is big enough for quicker generation
	//if ( strcmp ( cmd, "genclusterdb" ) == 0 ) {
	//	g_conf.m_clusterdbMinFilesToMerge = 20;
	//	// set up clusterdb
	//	g_conf.m_clusterdbMaxTreeMem = 50000000; // 50M
	//	g_conf.m_maxMem = 2000000000LL; // 2G
	//	g_mem.m_maxMem  = 2000000000LL; // 2G
	//}

	// site clusterdb
	if ( ! g_clusterdb.init()   ) {
		log("db: Clusterdb init failed." ); return 1; }
	// linkdb
	if ( ! g_linkdb.init()     ) {
		log("db: Linkdb init failed."   ); return 1; }

	// now clean the trees since all rdbs have loaded their rdb trees
	// from disk, we need to remove bogus collection data from teh trees
	// like if a collection was delete but tree never saved right it'll
	// still have the collection's data in it
	if ( ! g_collectiondb.addRdbBaseToAllRdbsForEachCollRec ( ) ) {
		log("db: Collectiondb init failed." ); return 1; }

	//Load the high-frequency term shortcuts (if they exist)
	g_hfts.load();
	
	// test all collection dirs for write permission
	int32_t pcount = 0;
	for ( int32_t i = 0 ; i < g_collectiondb.m_numRecs ; i++ ) {
		CollectionRec *cr = g_collectiondb.m_recs[i];
		if ( ! cr ) continue;
		if ( ++pcount >= 100 ) {
			log("rdb: not checking directory permission for "
			    "more than first 100 collections to save time.");
			break;
		}
		char tt[1024 + MAX_COLL_LEN ];
		sprintf ( tt , "%scoll.%s.%"INT32"",
			  g_hostdb.m_dir, cr->m_coll , (int32_t)cr->m_collnum );
		checkDirPerms ( tt ) ;
	}

	//
	// NOTE: ANYTHING THAT USES THE PARSER SHOULD GO BELOW HERE, UCINIT!
	//

	// load the appropriate dictionaries
	if ( ! g_speller.init() && g_conf.m_isLive ) {
		return 1;
	}

	// Load the category language table
	g_countryCode.loadHashTable();
	int32_t nce = g_countryCode.getNumEntries();

	if ( nce != 544729 ) {
		log("cat: unsupported catcountry.dat file with %"INT32" entries", nce);
	}

	// init the cache in Msg40 for caching search results
	// if cache not initialized now then do it now
	int32_t maxMem = g_conf.m_searchResultsMaxCacheMem;
	if ( ! g_genericCache[SEARCHRESULTS_CACHEID].init (
				     maxMem      ,   // max cache mem
				     -1          ,   // fixedDataSize
				     false       ,   // support lists of recs?
				     maxMem/2048 ,   // max cache nodes 
				     false       ,   // use half keys?
				     "results"   ,   // filename
				     //g_conf.m_searchResultsSaveCache ) ) {
				     true)){
		log("db: ResultsCache: %s",mstrerror(g_errno)); 
		return 1;
	}

	// init minsitenuminlinks buffer
	if ( ! g_tagdb.loadMinSiteInlinksBuffer() ) {
		log("db: failed to load sitelinks.txt data");
		return 1;
	}

	// . then our main udp server
	// . must pass defaults since g_dns uses it's own port/instance of it
	// . server should listen to a socket and register with g_loop
	// . sock read/write buf sizes are both 64000
	// . poll time is 60ms
	// . if the read/write bufs are too small it severely degrades
	//   transmission times for big messages. just use ACK_WINDOW *
	//   MAX_DGRAM_SIZE as the size so when sending you don't drop dgrams
	// . the 400k size allows us to cover Sync.cpp's activity well
	if ( ! g_udpServer.init( g_hostdb.getMyPort() ,&g_dp,
				 20000000 ,   // readBufSIze
				 20000000 ,   // writeBufSize
				 20       ,   // pollTime in ms
				 3500     ,   // max udp slots
				 false    )){ // is dns?
		log("db: UdpServer init failed." ); return 1; }
	// . this is the high priority udpServer, it's stuff is handled 1st
	//   sock read/write buf sizes are both almost 2 megs
	// . a niceness of -1 means its signal won't be blocked, real time
	// . poll time is 20ms
	//if ( ! g_udpServer2.init( g_hostdb.getMyPort2(),&g_dp,-1/*niceness*/,
	//			  10000000 ,   // readBufSIze
	//			  10000000 ,   // writeBufSize
	//			  20       ,   // pollTime in ms
	//			  1000     )){ // max udp slots
	//	log("db: UdpServer2 init failed." ); return 1; }

	// start pinging right away
	if ( ! g_pingServer.init() ) {
		log("db: PingServer init failed." ); return 1; }

	// start up repair loop
	if ( ! g_repair.init() ) {
		log("db: Repair init failed." ); return 1; }

	// start up repair loop
	if ( ! g_dailyMerge.init() ) {
		log("db: Daily merge init failed." ); return 1; }

	// . then dns Distributed client
	// . server should listen to a socket and register with g_loop
	// . Only the distributed cache shall call the dns server.
	if ( ! g_dns.init( h9->m_dnsClientPort ) ) {
		log("db: Dns distributed client init failed." ); return 1; }

	// . then dns Local client
	//if ( ! g_dnsLocal.init( 0 , false ) ) {
	//	log("db: Dns local client init failed." ); return 1; }

	// . then webserver
	// . server should listen to a socket and register with g_loop
	if ( ! g_httpServer.init( h9->m_httpPort, h9->m_httpsPort ) ) {
		log("db: HttpServer init failed. Another gb already "
		    "running?" ); 
		// this is dangerous!!! do not do the shutdown thing
		return 1;
		/*
		// just open a socket to port X and send GET /master?save=1
		if ( shutdownOldGB(h->m_httpPort) ) goto again;
		log("db: Shutdown failed.");
		resetAll();
		return 1;
		*/
	}

	if(!Msg1f::init()) {
		log("logviewer: init failed.");
		return 1;
	}

	// . now register all msg handlers with g_udp server
	if ( ! registerMsgHandlers() ) {
		log("db: registerMsgHandlers failed" ); return 1; }

	// save our rdbs every 5 seconds and save rdb if it hasn't dumped
	// in the last 10 mins
	//if ( ! g_loop.registerSleepCallback(5, NULL, saveRdbs ) ) {
	//	return log("db: save register failed"); return 1; }

	//
	// the new way to save all rdbs and conf
	//
	//g_process.init();

	// gb spellcheck
	if ( strcmp ( cmd , "spellcheck" ) == 0 ) {	
		if ( argc != cmdarg + 2 ) goto printHelp; // take no other args
		g_speller.test ( argv[cmdarg + 1] );
		return 0;
	}
	
	// gb dictLookupTest
	if ( strcmp ( cmd , "dictlookuptest" ) == 0 ) {	
		if ( argc != cmdarg + 2 ) goto printHelp; // take no other args
		g_speller.dictLookupTest ( argv[cmdarg + 1] );
		return 0;
	}

	// bdflush needs to be turned off because we need to control the
	// writes directly. we do this by killing the write thread.
	// we kill it when we need to do important reads, otherwise, if
	// we cannot control the writes it fucks up our reading.
	// no, now i use fsync(fd) in BigFile.cpp
	//log("WARNING: burstify bdflush with a "
	// "'echo 1 > /proc/sys/vm/bdflush' to optimize query response time "
	//    "during spidering.");
	//log("WARNING: mount with noatime option to speed up writes.");
	//log("         since we now call fsync(fd) after each write." );

	// debug msgs
	//log("REMINDER: make HOT again!");
	//log("REMINDER: reinsert thread call failed warning in BigFile.cpp.");
	//log("REMINDER: remove mem leack checking");
	//log("REMINDER: put thread back in Msg39");

	// . now m_minToMerge might have changed so try to do a merge
	// . only does one merge at a time
	// . other rdb's will sleep and retry until it's their turn
	//g_indexdb.getRdb()->m_minToMerge = 3;	
	//g_loop.registerSleepCallback ( 1000 ,
	//			       NULL ,
	//			       tryMergingWrapper );

	// . register a callback to try to merge everything every 2 seconds
	// . do not exit if we couldn't do this, not a huge deal
	// . put this in here instead of Rdb.cpp because we don't want
	//   generator commands merging on us
	// . the (void *)1 prevents gb from logging merge info every 2 seconds
	// . niceness is 1
	// BR: Upped from 2 sec to 60. No need to check for merge every 2 seconds.
	if ( ! g_loop.registerSleepCallback(60000,(void *)1,attemptMergeAll,1))
		log("db: Failed to init merge sleep callback.");

	// try to sync parms (and collection recs) with host 0
	if ( ! g_loop.registerSleepCallback(1000,NULL,tryToSyncWrapper,0))
		return false;

	if(g_recoveryMode) {
		//now that everything is init-ed send the message.
		char buf[256];
		log("admin: Sending emails.");
		sprintf(buf, "Host %"INT32" respawning after crash.(%s)",
			h9->m_hostId, iptoa(g_hostdb.getMyIp()));
		g_pingServer.sendEmail(NULL, buf);
	}

	if ( testMandrill ) {
		static EmailInfo ei;
		//ei.m_cr = g_collectiondb.getRec(1);
		ei.m_collnum = 1;
		ei.m_fromAddress.safePrintf("support@diffbot.com");
		ei.m_toAddress.safePrintf("matt@diffbot.com");
		ei.m_callback = exitWrapper;
		sendEmailThroughMandrill ( &ei );
		g_conf.m_spideringEnabled = false;
		g_conf.m_save = true;
	}

	// . start the spiderloop
	// . comment out when testing SpiderCache
	g_spiderLoop.startLoop();

	// allow saving of conf again
	g_conf.m_save = true;

	//spamTest();

	// flush stats
	//g_statsdb.flush();

	// ok, now activate statsdb
	g_statsdb.m_disabled = false;

	log("db: gb is now ready");

	// sync loop
	//if ( ! g_sync.init() ) {
	//	log("db: Sync init failed." ); return 1; } 
	// . now start g_loops main interrupt handling loop
	// . it should block forever
	// . when it gets a signal it dispatches to a server or db to handle it
	if ( ! g_loop.runLoop()    ) {
		log("db: runLoop failed." ); return 1; }
	// dummy return (0-->normal exit status for the shell)
	return 0;
}

/*
void spamTest ( ) {
	// quick test
	// load in sample
	char *filename = "/home/mwells/poo";
	int fd = open ( filename , O_RDONLY );
	char ppp[100000];
        struct stat stats;
        stat ( filename , &stats );
        int32_t size =  stats.st_size;
	if ( size > 100000 ) size = 99999;
	logf(LOG_INFO,"linkspam: Read %"INT32" bytes.",(int32_t)size);
	// copy errno to g_errno
	read ( fd , ppp  , size );
	ppp[size]=0;
	Xml xml;
	xml.set ( csUTF8,
		  ppp , 
		  size ,
		  false ,
		  size ,
		  false ,
		  TITLEREC_CURRENT_VERSION );
	Url linker;
	Url linkee;
	char *lee = "www.viagrapunch.com";
	linkee.set ( lee , gbstrlen ( lee ) );
	char *rr = "http://www.propeciauk.co.uk/links.htm";
	linker.set ( rr , gbstrlen(rr) );
	char *note = NULL;
        int32_t linkNode = -1;
	Links links;
	//int32_t siteFileNum = 48;//tr->getSiteFilenum();
        //Xml *sx = g_tagdb.getSiteXml ( siteFileNum, "main" , 4 );
        if (!links.set ( true , &xml , &linker , 
	false, // includeLinkHashes
                         true , // useBaseHref?
	TITLEREC_CURRENT_VERSION,
                         0 )) // niceness ))
                return;
	char linkText[1024];
	if ( linkNode < 0 )
		logf(LOG_INFO,"linkspam: linkee not found in content.");
        //int32_t linkTextLen = 
	links.getLinkText ( &linkee ,
			    linkText          ,
			    1023 ,
			    NULL,//&m_itemPtr          ,
			    NULL,//&m_itemLen          ,
			    &linkNode           ,
			    0 ); // niceness );
	bool ttt = isLinkSpam  ( &linker ,
				 NULL , //class TitleRec  *tr        ,
				 &xml ,
				 &links ,
				 size ,
				 &note ,
				 &linkee ,
				 linkNode  ,
				 "main" ,
				 0 ); // niceness
	logf(LOG_INFO,"linkspam: linkNode=%"INT32" val=%"INT32" note=%s",
	     linkNode,(int32_t)ttt,note);
	exit(0);
}
*/

int32_t checkDirPerms ( char *dir ) {
	if ( g_conf.m_readOnlyMode ) return 0;
	File f;
	f.set ( dir , "tmpfile" );
	if ( ! f.open ( O_RDWR | O_CREAT | O_TRUNC ) ) {
		log("disk: Unable to create %stmpfile. Need write permission "
		    "in this directory.",dir);
		return -1;
	}
	if ( ! f.unlink() ) {
		log("disk: Unable to delete %stmpfile. Need write permission "
		    "in this directory.",dir);
		return -1;
	}
	return 0;
}

// save them all
static       void doCmdAll   ( int fd, void *state ) ;
static       bool  s_sendToHosts;
static       bool  s_sendToProxies;
static       int32_t  s_hostId;
static       int32_t  s_hostId2;
static const char *s_cmd ;
static       char  s_buffer[128];
static HttpRequest s_r;
bool doCmd ( const char *cmd , int32_t hostId , char *filename , 
	     bool sendToHosts , bool sendToProxies , int32_t hostId2 ) {
	// need loop to work
	if ( ! g_loop.init() ) return log("db: Loop init failed." ); 
	// save it
	s_cmd = cmd;
	// we are no part of it
	//g_hostdb.m_hostId = -1;
	// pass it on
	s_hostId = hostId;
	s_sendToHosts = sendToHosts;
	s_sendToProxies = sendToProxies;
	s_hostId2 = hostId2;
	// set stuff so http server client-side works right
	g_conf.m_httpMaxSockets = 512;
	sprintf ( g_conf.m_spiderUserAgent ,"GigablastOpenSource/1.0");
	sprintf ( g_conf.m_spiderBotName ,"gigablastopensource");


	// register sleep callback to get started
	if ( ! g_loop.registerSleepCallback(1, NULL, doCmdAll , 0 ) )
		return log("admin: Loop init failed.");
	// not it
	log(LOG_INFO,"admin: broadcasting %s",cmd);
	// make a fake http request
	sprintf ( s_buffer , "GET /%s?%s HTTP/1.0" , filename , cmd );
	TcpSocket sock; sock.m_ip = 0;
	// make it local loopback so it passes the permission test in
	// doCmdAll()'s call to convertHttpRequestToParmList
	sock.m_ip = atoip("127.0.0.1");
	s_r.set ( s_buffer , gbstrlen ( s_buffer ) , &sock );
	// do not do sig alarms! for now just set this to null so 
	// the sigalarmhandler doesn't core
	//g_hostdb.m_myHost = NULL;
	// run the loop
	if ( ! g_loop.runLoop() ) 
		return log("INJECT: loop run failed.");
	return true;
}

void doneCmdAll ( void *state ) {
	log("cmd: completed command");
	exit ( 0 );
}


void doCmdAll ( int fd, void *state ) { 

	// do not keep calling it!
	g_loop.unregisterSleepCallback ( NULL, doCmdAll );

	// make port -1 to indicate none to listen on
	if ( ! g_udpServer.init( 18123 , // port to listen on
				 &g_dp,
				 20000000 ,   // readBufSIze
				 20000000 ,   // writeBufSize
				 20       ,   // pollTime in ms
				 3500     ,   // max udp slots
				 false    )){ // is dns?
		log("db: UdpServer init  on port 18123 failed: %s" ,
		    mstrerror(g_errno)); 
		exit(0);
	}

	// udpserver::sendRequest() checks we have a handle for msgs we send!
	// so fake it out with this lest it cores
	g_udpServer.registerHandler(0x3f,handleRequest3f);
	

	SafeBuf parmList;
	// returns false and sets g_errno on error
	if (!g_parms.convertHttpRequestToParmList(&s_r,&parmList,0,NULL)){
		log("cmd: error converting command: %s",mstrerror(g_errno));
		exit(0);
	}

	if ( parmList.length() <= 0 ) {
		log("cmd: no parmlist to send");
		exit(0);
	}

	// restrict broadcast to this hostid range!

	// returns true with g_errno set on error. uses g_udpServer
	if ( g_parms.broadcastParmList ( &parmList ,
					 NULL , 
					 doneCmdAll , // callback when done
					 s_sendToHosts ,
					 s_sendToProxies ,
					 s_hostId ,  // -1 means all
					 s_hostId2 ) ) { // -1 means all
		log("cmd: error sending command: %s",mstrerror(g_errno));
		exit(0);
		return;
	}
	// wait for it
	log("cmd: sent command");
}

// copy a collection from one network to another (defined by 2 hosts.conf's)
int collcopy ( char *newHostsConf , char *coll , int32_t collnum ) {
	Hostdb hdb;
	//if ( ! hdb.init(newHostsConf, 0/*assume we're zero*/) ) {
	if ( ! hdb.init( 0/*assume we're zero*/) ) {
		log("clusterCopy failed. Could not init hostdb with %s",
		    newHostsConf);
		return -1;
	}
	// sanity check
	if ( hdb.getNumShards() != g_hostdb.getNumShards() ) {
		log("Hosts.conf files do not have same number of groups.");
		return -1;
	}
	if ( hdb.getNumHosts() != g_hostdb.getNumHosts() ) {
		log("Hosts.conf files do not have same number of hosts.");
		return -1;
	}
	// host checks
	for ( int32_t i = 0 ; i < g_hostdb.m_numHosts ; i++ ) {
		Host *h = &g_hostdb.m_hosts[i];
		fprintf(stderr,"ssh %s '",iptoa(h->m_ip));
		fprintf(stderr,"du -skc %scoll.%s.%"INT32" | tail -1 '\n",
			h->m_dir,coll,collnum);
	}
	// loop over dst hosts
	for ( int32_t i = 0 ; i < g_hostdb.m_numHosts ; i++ ) {
		Host *h = &g_hostdb.m_hosts[i];
		// get the src host from the provided hosts.conf
		Host *h2 = &hdb.m_hosts[i];
		// print the copy
		//fprintf(stderr,"rcp %s:%s*db*.dat* ",
		//	iptoa( h->m_ip), h->m_dir  );
		fprintf(stderr,"nohup ssh %s '",iptoa(h->m_ip));
		fprintf(stderr,"rcp -r ");
		fprintf(stderr,"%s:%scoll.%s.%"INT32" ",
			iptoa(h2->m_ip), h2->m_dir , coll, collnum );
		fprintf(stderr,"%s' &\n", h->m_dir  );
		//fprintf(stderr," rcp -p %s*.map* ", h->m_dir );
		//fprintf(stderr," rcp -r %scoll.* ", h->m_dir );
		//fprintf(stderr,"%s:%s " ,iptoa(h2->m_ip), h2->m_dir );
	}
	return 1;
}

// generate the copies that need to be done to scale from oldhosts.conf
// to newhosts.conf topology.
int scale ( char *newHostsConf , bool useShotgunIp) {

	g_hostdb.resetPortTables();

	Hostdb hdb;
	//if ( ! hdb.init(newHostsConf, 0/*assume we're zero*/) ) {
	if ( ! hdb.init( 0/*assume we're zero*/) ) {
		log("Scale failed. Could not init hostdb with %s",
		    newHostsConf);
		return -1;
	}

	// ptrs to the two hostdb's
	Hostdb *hdb1 = &g_hostdb;
	Hostdb *hdb2 = &hdb;

	// this function was made to scale UP, but if scaling down
	// then swap them!
	if ( hdb1->m_numHosts > hdb2->m_numHosts ) {
		Hostdb *tmp = hdb1;
		hdb1 = hdb2;
		hdb2 = tmp;
	}

	// . ensure old hosts in g_hostdb are in a derivate groupId in
	//   newHostsConf
	// . old hosts may not even be present! consider them the same host,
	//   though, if have same ip and working dir, because that would
	//   interfere with a file copy.
	for ( int32_t i = 0 ; i < hdb1->m_numHosts ; i++ ) {
	Host *h = &hdb1->m_hosts[i];
	// look in new guy
	for ( int32_t j = 0 ; j < hdb2->m_numHosts ; j++ ) {
		Host *h2 = &hdb2->m_hosts[j];
		// if a match, ensure same group
		if ( h2->m_ip != h->m_ip ) continue;
		if ( strcmp ( h2->m_dir , h->m_dir ) != 0 ) continue;
	}
	}

	// . ensure that:
	//   (h2->m_groupId & (hdb1->m_numGroups -1)) == h->m_groupId 
	//   where h2 is in a derivative group of h.
	// . do a quick monte carlo test to make sure that a key in old
	//   group #0 maps to groups 0,8,16,24 for all keys and all dbs
	uint32_t shard1;
	uint32_t shard2;
	for ( int32_t i = 0 ; i < 1000 ; i++ ) {
		//key_t k;
		//k.n1 = rand(); k.n0 = rand(); k.n0 <<= 32; k.n0 |= rand();
		//key128_t k16;
		//k16.n0 = k.n0;
		//k16.n1 = rand(); k16.n1 <<= 32; k16.n1 |= k.n1;
		char k[MAX_KEY_BYTES];
		for ( int32_t ki = 0 ; ki < MAX_KEY_BYTES ; ki++ )
			k[ki] = rand() & 0xff;

		// get old group (groupId1) and new group (groupId2)
		shard1 = hdb1->getShardNum ( RDB_TITLEDB , k );//, hdb1 );
		shard2 = hdb2->getShardNum( RDB_TITLEDB , k );//, hdb2 );
	}

	// . now copy all titleRecs in old hosts to all derivatives
	// . going from 8 (3bits) hosts to 32 (5bits), for instance, old 
	//   group id #0 would copy to group ids 0,8,16 and 24.
	// . 000 --> 00000(#0), 01000(#8), 10000(#16), 11000(#24)
	// . titledb determine groupId by mod'ding the docid
	//   contained in their most significant key bits with the number
	//   of groups.  see Titledb.h::getGroupId(docid)
	// . indexdb and tagdb mask the hi bits of the key with 
	//   hdb1->m_groupMask, which is like a reverse mod'ding:
	//   000 --> 00000, 00001, 00010, 00011
	char done [ 8196 ];
	memset ( done , 0 , 8196 );
	for ( int32_t i = 0 ; i < hdb1->m_numHosts ; i++ ) {
	Host *h = &hdb1->m_hosts[i];
	char flag = 0;
	// look in new guy
	for ( int32_t j = 0 ; j < hdb2->m_numHosts ; j++ ) {
		Host *h2 = &hdb2->m_hosts[j];
		// do not copy to oneself
		if ( h2->m_ip == h->m_ip &&
		     strcmp ( h2->m_dir , h->m_dir ) == 0 ) continue;
		// skip if not derivative groupId for titledb
		//if ( (h2->m_groupId & hdb1->m_groupMask) !=
		//     h->m_groupId ) continue;
		// continue if already copying to here
		if ( done[j] ) continue;
		// mark as done
		done[j] = 1;

		// skip local copies for now!!
		//if ( h->m_ip == h2->m_ip ) continue;

		// use ; separator
		if ( flag ) fprintf(stderr,"; ");
		//else        fprintf(stderr,"ssh %s \"",iptoa(h->m_ip));
		else        fprintf(stderr,"ssh %s \"",h->m_hostname);
		// flag
		flag = 1;
		// print the copy
		//fprintf(stderr,"rcp %s:%s*db*.dat* ",
		//	iptoa( h->m_ip), h->m_dir  );
		// if same ip then do a 'cp' not rcp
		char *cmd = "rcp -r";
		if ( h->m_ip == h2->m_ip ) cmd = "cp -pr";

		fprintf(stderr,"%s %s*db*.dat* ", cmd, h->m_dir  );

		if ( h->m_ip == h2->m_ip )
			fprintf(stderr,"%s ;", h2->m_dir );
		else {
			//int32_t ip = h2->m_ip;
			//if ( useShotgunIp ) ip = h2->m_ipShotgun;
			//fprintf(stderr,"%s:%s ;",iptoa(ip), h2->m_dir );
			char *hn = h2->m_hostname;
			if ( useShotgunIp ) hn = h2->m_hostname;//2
			fprintf(stderr,"%s:%s ;",hn, h2->m_dir );

		}

		//fprintf(stderr," rcp -p %s*.map* ", h->m_dir );
		fprintf(stderr," %s %scoll.* ", cmd, h->m_dir );

		if ( h->m_ip == h2->m_ip )
			fprintf(stderr,"%s " , h2->m_dir );
		else {
			//int32_t ip = h2->m_ip;
			//if ( useShotgunIp ) ip = h2->m_ipShotgun;
			//fprintf(stderr,"%s:%s " ,iptoa(ip), h2->m_dir );
			char *hn = h2->m_hostname;
			if ( useShotgunIp ) hn = h2->m_hostname;//2;
			fprintf(stderr,"%s:%s " ,hn, h2->m_dir );
		}

		/*
		fprintf(stderr,"scp %s:%s/titledb* %s:%s\n",
			iptoa( h->m_ip), h->m_dir  ,
			iptoa(h2->m_ip), h2->m_dir );
		fprintf(stderr,"scp %s:%s/indexdb* %s:%s\n",
			iptoa( h->m_ip), h->m_dir  ,
			iptoa(h2->m_ip), h2->m_dir );
		fprintf(stderr,"scp %s:%s/spiderdb* %s:%s\n",
			iptoa( h->m_ip), h->m_dir  ,
			iptoa(h2->m_ip), h2->m_dir );
		fprintf(stderr,"scp %s:%s/clusterdb* %s:%s\n",
			iptoa( h->m_ip), h->m_dir  ,
			iptoa(h2->m_ip), h2->m_dir );
		fprintf(stderr,"scp %s:%s/tagdb* %s:%s\n",
			iptoa( h->m_ip), h->m_dir  ,
			iptoa(h2->m_ip), h2->m_dir );
		*/
	}
	if ( flag ) fprintf(stderr,"\" &\n");
	}
	return 1;
}


static int install_file(const char *dst_host, const char *src_file, const char *dst_file)
{
	char cmd[1024];
	sprintf(cmd, "scp -p %s %s:%s",
		src_file,
		dst_host,
		dst_file);
	log(LOG_INIT,"admin: %s", cmd);
	int rc = system(cmd);
	return rc;
}


static int install_file(const char *file)
{
	for ( int32_t i = 0 ; i < g_hostdb.getNumHosts() ; i++ ) {
		Host *h2 = g_hostdb.getHost(i);
		if(h2==g_hostdb.getMyShard())
			continue; //skip ourselves
		char full_dst_file[1024];
		sprintf(full_dst_file, "%s%s",h2->m_dir,file);
		install_file(iptoa(h2->m_ip),
		             file,
	                     full_dst_file);
	}
	return 0; //return value is unclear
}


// installFlag is 1 if we are really installing, 2 if just starting up gb's
// installFlag should be a member of the ifk_ enum defined above
int install ( install_flag_konst_t installFlag , int32_t hostId , char *dir , 
	      char *coll , int32_t hostId2 , char *cmd ) {

	// use hostId2 to indicate the range hostId-hostId2, but if it is -1
	// then it was not given, so restrict to just hostId
	if ( hostId2 == -1 ) hostId2 = hostId;

	char tmp[1024];

	if ( installFlag == ifk_proxy_start ) {
		for ( int32_t i = 0; i < g_hostdb.m_numProxyHosts; i++ ) {
			Host *h2 = g_hostdb.getProxy(i);
			// limit install to this hostId if it is >= 0
			if ( hostId >= 0 && h2->m_hostId != hostId ) continue;

			// . save old log now, too
			char tmp2[1024];
			tmp2[0]='\0';
			// let's do this for everyone now
			//if ( h2->m_hostId == 0 )
			sprintf(tmp2,
				"mv ./proxylog ./proxylog-bak`date '+"
				"%%Y%%m%%d-%%H%%M%%S'` ; " );
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; %s"
				"./gb proxy load %"INT32" >& ./proxylog &\" &",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				tmp2           ,
				i);
			// log it
			log(LOG_INIT,"%s", tmp);
			// execute it
			int32_t ret = system ( tmp );
			if ( ret < 0 ) {
				fprintf(stderr,"Error loading proxy: %s\n",
					mstrerror(errno));
				exit(-1);
			}
			fprintf(stderr,"If proxy does not start, make sure "
				"its ip is correct in hosts.conf\n");
		}
		return 0;
	}

	if ( installFlag == ifk_proxy_kstart ) {
		for ( int32_t i = 0; i < g_hostdb.m_numProxyHosts; i++ ) {
			Host *h2 = g_hostdb.getProxy(i);
			// limit install to this hostId if it is >= 0
			if ( hostId >= 0 && h2->m_hostId != hostId ) continue;

			// . save old log now, too
			//char tmp2[1024];
			//tmp2[0]='\0';
			// let's do this for everyone now
			//if ( h2->m_hostId == 0 )
			//sprintf(tmp2,
			//	"mv ./proxylog ./proxylog-`date '+"
			//	"%%Y_%%m_%%d-%%H:%%M:%%S'` ; " );
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			//to test add: ulimit -t 10; to the ssh cmd
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"export MALLOC_CHECK_=0;"
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; "
				"ADDARGS='' ; "
				"EXITSTATUS=1 ; "
				"while [ \\$EXITSTATUS != 0 ]; do "
 				"{ "
				//"mv ./proxylog ./proxylog-\\`date '+"
				//"%%Y_%%m_%%d-%%H:%%M:%%S'\\` ; " 
				"./gb proxy load %"INT32" " // mdw
				"\\$ADDARGS "
				" >& ./proxylog ;"
				"EXITSTATUS=\\$? ; "
				"ADDARGS='-r' ; "
				"} " 
 				"done >& /dev/null & \" & ",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				h2->m_hostId   );
			// log it
			log(LOG_INIT,"admin: %s", tmp);
			// execute it
			int32_t ret = system ( tmp );
			if ( ret < 0 ) {
				fprintf(stderr,"Error loading proxy: %s\n",
					mstrerror(errno));
				exit(-1);
			}
			fprintf(stderr,"If proxy does not start, make sure "
				"its ip is correct in hosts.conf\n");
		}
		return 0;
	}

	HashTableX iptab;
	char tmpBuf[2048];
	iptab.set(4,4,64,tmpBuf,2048,true,0,"iptsu");

	int32_t maxOut = 500;

	// this is a big scp so only do two at a time...
	if  ( installFlag == ifk_install ) maxOut = 1;

	// same with this. takes too long on gk144, jams up
	if  ( installFlag == ifk_installgb ) maxOut = 4;

	if  ( installFlag == ifk_installgbrcp ) maxOut = 4;

	//int32_t maxOutPerIp = 6;

	// go through each host
	for ( int32_t i = 0 ; i < g_hostdb.getNumHosts() ; i++ ) {
		Host *h2 = g_hostdb.getHost(i);

		char *amp = " ";

		// if i is NOT multiple of maxOut then use '&'
		// even if all all different machines (IPs) scp chokes and so
		// does rcp a little. so restrict to maxOut at a time.
		if ( (i+1) % maxOut ) amp = "&";
			

		// if host ip is like the 10th occurence then do
		// not do ampersand. this is for hosts on the same IP.
		//int32_t score = iptab.getScore32(&h2->m_ip);
		//if ( (score % maxOutPerIp) ) amp = "&";
		//iptab.addScore((int32_t *)&h2->m_ip);

		// limit install to this hostId if it is >= 0
		//if ( hostId >= 0 && h2->m_hostId != hostId ) continue;
		if ( hostId >= 0 && hostId2 == -1 ) {
			if ( h2->m_hostId != hostId ) continue;
		}
		// if doing a range of hostid, hostId2 is >= 0
		else if ( hostId >= 0 && hostId2 >= 0 ) {
			if ( h2->m_hostId < hostId  ) continue;
			if ( h2->m_hostId > hostId2 ) continue;
		}
		// do not install to self
		//if ( h2->m_hostId == g_hostdb.m_hostId ) continue;
		// backupcopy
		if ( installFlag == ifk_backupcopy ) {
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"mkdir %s ; "
				"cp -ai *.dat* *.map gb.conf "
				"hosts.conf %s\" &",
				iptoa(h2->m_ip), h2->m_dir , dir , dir );
			// log it
			log ( "%s", tmp);
			// execute it
			system ( tmp );
			continue;
		}
		// backupmove
		if ( installFlag == ifk_backupmove ) {
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"mkdir %s ; "
				"mv -i *.dat* *.map "
				"%s\" &",
				iptoa(h2->m_ip), h2->m_dir , dir , dir );
			// log it
			log ( "%s", tmp);
			// execute it
			system ( tmp );
			continue;
		}
		// backuprestore
		if ( installFlag == ifk_backuprestore ) {
			sprintf(tmp,
				"ssh %s \"cd %s ; cd %s ; "
				"mv -i *.dat* *.map gb.conf "
				"hosts.conf %s\" &",
				iptoa(h2->m_ip), h2->m_dir , dir , h2->m_dir );
			// log it
			log ( "%s", tmp);
			// execute it
			system ( tmp );
			continue;
		}

		// dumpmissing logic
		else if ( installFlag == ifk_dumpmissing ) {
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; "
				"./gb dumpmissing %s %"INT32" "
				">& ./missing%"INT32" &\" &",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				//h2->m_dir      ,
				coll           ,
				h2->m_hostId   ,
				h2->m_hostId   );
			// log it
			log(LOG_INIT,"admin: %s", tmp);
			// execute it
			system ( tmp );
		}
		else if ( installFlag == ifk_dumpdups ) {
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; "
				"./gb dumpdups %s %"INT32" "
				">& ./dups%"INT32" &\" &",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				//h2->m_dir      ,
				coll           ,
				h2->m_hostId   ,
				h2->m_hostId   );
			// log it
			log(LOG_INIT,"admin: %s", tmp);
			// execute it
			system ( tmp );
		}
		// removedocids logic
		else if ( installFlag == ifk_removedocids ) {
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; "
				// hostid is now inferred from path
				"./gb "//%"INT32" "
				"removedocids %s %s %"INT32" "
				">& ./removelog%03"INT32" &\" &",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				//h2->m_dir      ,
				//h2->m_hostId   ,
				coll           ,
				dir            , // really docidsFile
				h2->m_hostId   ,
				h2->m_hostId   );
			// log it
			log(LOG_INIT,"admin: %s", tmp);
			// execute it
			system ( tmp );
		}

		char *dir = "./";
		// install to it
		if      ( installFlag == ifk_install ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;

			char *srcDir = "./";
			SafeBuf fileListBuf;
			g_process.getFilesToCopy ( srcDir , &fileListBuf );

			// include this one as well for install
			//fileListBuf.safePrintf(" %shosts.conf",srcDir);
			// the dmoz data dir if there
			fileListBuf.safePrintf(" %scat",srcDir);
			fileListBuf.safePrintf(" %shosts.conf",srcDir);
			fileListBuf.safePrintf(" %sgb.conf",srcDir);

			char *ipStr = iptoa(h2->m_ip);

			SafeBuf tmpBuf;
			tmpBuf.safePrintf(
					  // ensure directory is there, if
					  // not then make it
					  "ssh %s 'mkdir %s' ; "
					  "scp -p -r %s %s:%s"
					  , ipStr
					  , h2->m_dir

					  , fileListBuf.getBufStart()
					  , iptoa(h2->m_ip)
					  , h2->m_dir
					  );
			char *tmp = tmpBuf.getBufStart();
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
		else if ( installFlag == ifk_installgb ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;

			File f;
			char *target = "gb.new";
			f.set(g_hostdb.m_myHost->m_dir,target);
			if ( ! f.doesExist() ) target = "gb";

			sprintf(tmp,
				"scp -p " // blowfish is faster
				"%s%s "
				"%s:%s/gb.installed%s",
				dir,
				target,
				iptoa(h2->m_ip),
				h2->m_dir,
				amp);
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
		else if ( installFlag == ifk_installgbrcp ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;

			File f;
			char *target = "gb.new";
			f.set(g_hostdb.m_myHost->m_dir,target);
			if ( ! f.doesExist() ) target = "gb";

			sprintf(tmp,
				"rcp "
				"%s%s "
				"%s:%s/gb.installed%s",
				dir,
				target,
				iptoa(h2->m_ip),
				h2->m_dir,
				amp);
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
		else if ( installFlag == ifk_installtmpgb ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;
			sprintf(tmp,
				"scp -p "
				"%sgb.new "
				"%s:%s/tmpgb.installed &",
				dir,
				iptoa(h2->m_ip),
				h2->m_dir);
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
		else if ( installFlag == ifk_installconf ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;
			sprintf(tmp,
				"scp -p %sgb.conf %shosts.conf %s:%s %s",
				dir ,
				dir ,
				//h->m_hostId ,
				iptoa(h2->m_ip),
				h2->m_dir,
				//h2->m_hostId);
				amp);

			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
			// sprintf(tmp,
			// 	"scp %shosts.conf %s:%shosts.conf &",
			// 	dir ,
			// 	iptoa(h2->m_ip),
			// 	h2->m_dir);
			// log(LOG_INIT,"admin: %s", tmp);
			// system ( tmp );
			// sprintf(tmp,
			// 	"scp %shosts2.conf %s:%shosts2.conf &",
			// 	dir ,
			// 	iptoa(h2->m_ip),
			// 	h2->m_dir);
			// log(LOG_INIT,"admin: %s", tmp);
			// system ( tmp );
		}
		else if ( installFlag == ifk_start ) {
			// . save old log now, too
			//char tmp2[1024];
			//tmp2[0]='\0';
			// let's do this for everyone now
			//if ( h2->m_hostId == 0 )
			//sprintf(tmp2,
			//	"mv ./log%03"INT32" ./log%03"INT32"-`date '+"
			//	"%%Y_%%m_%%d-%%H:%%M:%%S'` ; " ,
			//	h2->m_hostId   ,
			//	h2->m_hostId   );
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			sprintf(tmp,
				"ssh %s \"cd %s ; ulimit -c unlimited; "
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; " // %s"
				//"./gb %"INT32" >& ./log%03"INT32" &\" %s",
				// without "sleep 1" ssh seems to exit
				// bash before it can start gb and gb does
				// not start up.
				// hostid is now inferred from path.
				"./gb & sleep 1\" %s",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				//tmp2           ,
				//h2->m_dir      ,
				//h2->m_hostId   ,
				//h2->m_hostId   ,
				amp);
			// log it
			//log(LOG_INIT,"admin: %s", tmp);
			fprintf(stdout,"admin: %s\n", tmp);
			// execute it
			system ( tmp );
		}
		// start up a dummy cluster using hosts.conf ports + 1
		else if ( installFlag == ifk_tmpstart ) {
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			sprintf(tmp,
				"ssh %s \"cd %s ; "
				"cp -f tmpgb tmpgb.oldsave ; "
				"mv -f tmpgb.installed tmpgb ; "
				"%s/tmpgb tmpstarthost "
				"%"INT32" >& ./tmplog%03"INT32" &\" &",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				h2->m_dir      ,
				h2->m_hostId   ,
				h2->m_hostId   );
			// log it
			log(LOG_INIT,"admin: %s", tmp);
			// execute it
			system ( tmp );
		}
		else if ( installFlag == ifk_kstart ) {
			//keepalive
			// . save old log now, too
			//char tmp2[1024];
			//tmp2[0]='\0';
			// let's do this for everyone now
			//if ( h2->m_hostId == 0 )
			// we do not run as daemon so keepalive loop will
			// work properly...
			//sprintf(tmp2,
			//	"mv ./log%03"INT32" ./log%03"INT32"-`date '+"
			//	"%%Y_%%m_%%d-%%H:%%M:%%S'` ; " ,
			//	h2->m_hostId   ,
			//	h2->m_hostId   );
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			//to test add: ulimit -t 10; to the ssh cmd
			sprintf(tmp,
				"ssh %s \"cd %s ; ulimit -c unlimited; "
				"export MALLOC_CHECK_=0;"
				"cp -f gb gb.oldsave ; "
				"ADDARGS='' "
				"INC=1 "
				"EXITSTATUS=1 ; "
				 "while [ \\$EXITSTATUS != 0 ]; do "
 				 "{ "

				// if gb still running, then do not try to
				// run it again. we
				// probably double-called './gb start'.
				// so see if the port is bound to.
				// "./gb isportinuse %i ; "
				// "if [ \\$? -eq 1 ] ; then "
				// "echo \"gb or something else "
				// "is already running on "
				// "port %i. Not starting.\" ; "
				// "exit 0; "
				// "fi ; "

				// ok, the port is available
				//"echo \"Starting gb\"; "

				//"exit 0; "

				// in case gb was updated...
				"mv -f gb.installed gb ; "

				// move the log file
				"mv ./log%03"INT32" ./log%03"INT32"-bak\\`date '+"
				"%%Y%%m%%d-%%H%%M%%S'\\` ; " 

				// indicate -l so we log to a logfile
				"./gb -l "//%"INT32" "
				"\\$ADDARGS "

				// no longer log to stderr so we can
				// do log file rotation
				//" >& ./log%03"INT32""
				" ;"

				"EXITSTATUS=\\$? ; "
				"ADDARGS='-r'\\$INC ; "
				"INC=\\$((INC+1));"
				"} " 
				"done > /dev/null 2>&1 & \" %s",
 				//"done & \" %s",
				//"\" %s",
				iptoa(h2->m_ip),
				h2->m_dir      ,

				// for ./gb isportinuse %i
				// h2->m_httpPort ,
				// h2->m_httpPort ,

				// for moving log file
				 h2->m_hostId   ,
				 h2->m_hostId   ,

				//h2->m_dir      ,

				// hostid is now inferred from path
				//h2->m_hostId   ,
				amp );

			// log it
			//log(LOG_INIT,"admin: %s", tmp);
			fprintf(stdout,"admin: %s\n", tmp);
			// execute it
			system ( tmp );
		}
		else if ( installFlag == ifk_dstart ) {
			//keepalive
			// . save old log now, too
			//char tmp2[1024];
			//tmp2[0]='\0';
			// let's do this for everyone now
			//if ( h2->m_hostId == 0 )
			// we do not run as daemon so keepalive loop will
			// work properly...
			//sprintf(tmp2,
			//	"mv ./log%03"INT32" ./log%03"INT32"-`date '+"
			//	"%%Y_%%m_%%d-%%H:%%M:%%S'` ; " ,
			//	h2->m_hostId   ,
			//	h2->m_hostId   );
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			amp = "&";
			//if ( i > 0 && (i%5) == 0 ) amp = "";
			//to test add: ulimit -t 10; to the ssh cmd
			sprintf(tmp,
				"ssh %s \"cd %s ; ulimit -c unlimited; "
				"export MALLOC_CHECK_=0;"
				"cp -f gb gb.oldsave ; "
				"mv -f gb.installed gb ; "
				//"ADDARGS='' ; "
				//"EXITSTATUS=1 ; "
				// "while [ \\$EXITSTATUS != 0 ]; do "
 				// "{ "

				// move the log file
				//"mv ./log%03"INT32" ./log%03"INT32"-\\`date '+"
				//"%%Y_%%m_%%d-%%H:%%M:%%S'\\` ; "

				"./gb -d "//%"INT32" "
				//"\\$ADDARGS "
				//" ;"
				//" >& ./log%03"INT32" ;"

				//"EXITSTATUS=\\$? ; "
				//"ADDARGS='-r' ; "
				//"} "
 				//"done >& /dev/null & \" %s",
				"\" %s",
				iptoa(h2->m_ip),
				h2->m_dir      ,

				// for moving log file
				// h2->m_hostId   ,
				// h2->m_hostId   ,

				//h2->m_dir      ,

				// hostid is now inferred from path
				//h2->m_hostId   ,
				amp );

			// log it
			//log(LOG_INIT,"admin: %s", tmp);
			fprintf(stdout,"admin: %s\n", tmp);
			// execute it
			system ( tmp );
		}
		else if ( installFlag == ifk_genclusterdb ) {
			// . save old log now, too
			char tmp2[1024];
			tmp2[0]='\0';
			// let's do this for everyone now
			//if ( h2->m_hostId == 0 )
			//sprintf(tmp2,
			//	"mv ./log%03"INT32" ./log%03"INT32"-`date '+"
			//	"%%Y_%%m_%%d-%%H:%%M:%%S'` ; " ,
			//	h2->m_hostId   ,
			//	h2->m_hostId   );
			// . assume conf file name gbHID.conf
			// . assume working dir ends in a '/'
			sprintf(tmp,
				"ssh %s \"cd %s ;"
				//"%s"
				"./gb genclusterdb %s %"INT32" >&"
				"./log%03"INT32"-genclusterdb &\" &",
				iptoa(h2->m_ip),
				h2->m_dir      ,
				//h2->m_dir      ,
				//tmp2           ,
				coll           ,
				h2->m_hostId   ,
				h2->m_hostId   );
			// log it
			log(LOG_INIT,"admin: %s", tmp);
			// execute it
			system ( tmp );
		}
		// dsh
		else if ( installFlag == ifk_dsh ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;
			sprintf(tmp,
				"ssh %s 'cd %s ; %s' %s",
				iptoa(h2->m_ip),
				h2->m_dir,
				cmd ,
				amp );
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
		// dsh2
		else if ( installFlag == ifk_dsh2 ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;
			//sprintf(tmp,
			//	"ssh %s '%s' &",
			//	iptoa(h2->m_ipShotgun),
			//	cmd );
			sprintf(tmp,
				"ssh %s 'cd %s ; %s'",
				iptoa(h2->m_ip),
				h2->m_dir,
				cmd );
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
		// installconf2
		else if ( installFlag == ifk_installconf2 ) {
			// don't copy to ourselves
			//if ( h2->m_hostId == h->m_hostId ) continue;
			sprintf(tmp,
				"rcp %sgb.conf %shosts.conf %shosts2.conf "
				"%s:%s &",
				dir ,
				dir ,
				dir ,
				//h->m_hostId ,
				iptoa(h2->m_ipShotgun),
				h2->m_dir);
				//h2->m_hostId);
			log(LOG_INIT,"admin: %s", tmp);
			system ( tmp );
		}
	}
	// return 0 on success
	return 0;
}

bool registerMsgHandlers ( ) {
	if (! registerMsgHandlers1()) return false;
	if (! registerMsgHandlers2()) return false;
	if (! registerMsgHandlers3()) return false;
	if ( ! g_pingServer.registerHandler() ) return false;

	// in SpiderProxy.cpp...
	initSpiderProxyStuff();
	return true;
}

bool registerMsgHandlers1(){
	Msg20 msg20;	if ( ! msg20.registerHandler () ) return false;
	MsgC  msgC ;    if ( ! msgC.registerHandler  () ) return false;

	if ( ! Msg22::registerHandler() ) return false;

	return true;
}

bool registerMsgHandlers2(){
	Msg0  msg0 ;	if ( ! msg0.registerHandler  () ) return false;
	Msg1  msg1 ;	if ( ! msg1.registerHandler  () ) return false;

	if ( ! Msg13::registerHandler() ) return false;

	if ( ! g_udpServer.registerHandler(0xc1,handleRequestc1)) return false;
	if ( ! g_udpServer.registerHandler(0x39,handleRequest39)) return false;
	//if ( ! g_udpServer.registerHandler(0x2c,handleRequest2c)) return false;
	if ( ! g_udpServer.registerHandler(0x12,handleRequest12)) return false;

	if ( ! registerHandler4  () ) return false;

	if(! g_udpServer.registerHandler(0x3e,handleRequest3e)) return false;
	if(! g_udpServer.registerHandler(0x3f,handleRequest3f)) return false;

	if ( ! g_udpServer.registerHandler(0x25,handleRequest25)) return false;
	if ( ! g_udpServer.registerHandler(0x07,handleRequest7)) return false;

	return true;
}

bool registerMsgHandlers3(){
	Msg17 msg17;    if ( ! msg17.registerHandler () ) return false;
	if ( ! Msg40::registerHandler() ) return false;
	return true;
}

bool mainShutdown ( bool urgent ) {
	return g_process.shutdown(urgent);
}

#include "Rdb.h"
#include "Xml.h"
#include "Threads.h"

//
// dump routines here now
//

void dumpTitledb (char *coll,int32_t startFileNum,int32_t numFiles,bool includeTree,
                  int64_t docid , bool justPrintDups) {

	if(startFileNum!=0 && numFiles<0) {
		//this may apply to all files, but I haven't checked into hash-based ones yet
		fprintf(stderr,"If <startFileNum> is specified then <numFiles> must be too\n");
		return;
	}
	if (!ucInit(g_hostdb.m_dir)) {
		log("Unicode initialization failed!");
		return;
	}
	// init our table for doing zobrist hashing
	if ( ! hashinit() ) {
		log("db: Failed to init hashtable." ); return ; }
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	//g_conf.m_spiderdbMaxDiskPageCacheMem   = 0;
	g_titledb.init ();
	//g_collectiondb.init(true);
	g_titledb.getRdb()->addRdbBase1(coll);
	key_t startKey ;
	key_t endKey   ;
	key_t lastKey  ;
	startKey.setMin();
	endKey.setMax();
	lastKey.setMin();
	startKey = g_titledb.makeFirstKey ( docid );
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;
	Msg5 msg5;
	Msg5 msg5b;
	RdbList list;
	int64_t prevId = 0LL;
	int32_t count = 0;
	char ttt[2048+MAX_URL_LEN];
	HashTableX dedupTable;
	dedupTable.set(4,0,10000,NULL,0,false,0,"maintitledb");
	//g_synonyms.init();
	// load the appropriate dictionaries -- why???
	//g_speller.init(); 

	// make this
	XmlDoc *xd;
	try { xd = new (XmlDoc); }
	catch ( ... ) {
		fprintf(stdout,"could not alloc for xmldoc\n");
		exit(-1);
	}
	CollectionRec *cr = g_collectiondb.getRec(coll);
	if(cr==NULL) {
		fprintf(stderr,"Unknown collection '%s'\n", coll);
		return;
	}

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_TITLEDB   ,
			      cr->m_collnum          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         , // err correction?
			      NULL          , // cache key ptr
			      0             , // retry num
			      -1            , // maxRetries
			      true          , // compensate for merge
			      -1LL          , // sync point
			      &msg5b        )){
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;

	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k       = list.getCurrentKey();
		char *rec     = list.getCurrentRec();
		int32_t  recSize = list.getCurrentRecSize();
		int64_t docId       = g_titledb.getDocIdFromKey ( k );
		//int32_t      hostHash    = g_titledb.getHostHash     ( k );
		//int32_t      contentHash = g_titledb.getContentHash  ( k );
		if ( k <= lastKey ) 
			log("key out of order. "
			    "lastKey.n1=%"XINT32" n0=%"XINT64" "
			    "currKey.n1=%"XINT32" n0=%"XINT64" ",
			    lastKey.n1,lastKey.n0,
			    k.n1,k.n0);
		lastKey = k;
		int32_t shard = g_hostdb.getShardNum ( RDB_TITLEDB , &k );
		// print deletes
		if ( (k.n0 & 0x01) == 0) {
			fprintf(stdout,"n1=%08"XINT32" n0=%016"XINT64" docId=%012"INT64" "
			       "shard=%"INT32" (del)\n", 
				k.n1 , k.n0 , docId , shard );
			continue;
		}
		// free the mem
		xd->reset();
		// uncompress the title rec
		//TitleRec tr;
		if ( ! xd->set2 ( rec , recSize , coll ,NULL , 0 ) )
			continue;

		// get this
		//uint32_t siteHash32 = xd->m_siteHash32;

		// extract the url
		Url *u = xd->getFirstUrl();

		// MOD to only print root urls
		//if (!u->isRoot()) continue;
		// get ip 
		char ipbuf [ 32 ];
		strcpy ( ipbuf , iptoa(u->getIp() ) );
		// pad with spaces
		int32_t blen = gbstrlen(ipbuf);
		while ( blen < 15 ) ipbuf[blen++]=' ';
		ipbuf[blen]='\0';
		//int32_t nc = xd->size_catIds / 4;//tr.getNumCatids();
		if ( justPrintDups ) {
			// print into buf
			if ( docId != prevId ) {
				time_t ts = xd->m_spideredTime;//tr.getSpiderDa
				struct tm *timeStruct = localtime ( &ts );
				//struct tm *timeStruct = gmtime ( &ts );
				char ppp[100];
				strftime(ppp,100,"%b-%d-%Y-%H:%M:%S",
					 timeStruct);
				LinkInfo *info = xd->ptr_linkInfo1;//tr.ge
				char foo[1024];
				foo[0] = '\0';
				//if ( tr.getVersion() >= 86 ) 
				sprintf(foo,
					//"tw=%"INT32" hw=%"INT32" upw=%"INT32" "
					"sni=%"INT32" ",
					//(int32_t)xd->m_titleWeight,
					//(int32_t)xd->m_headerWeight,
					//(int32_t)xd->m_urlPathWeight,
					(int32_t)xd->m_siteNumInlinks);
				char *ru = xd->ptr_redirUrl;
				if ( ! ru ) ru = "";
				sprintf(ttt,
					"n1=%08"XINT32" n0=%016"XINT64" docId=%012"INT64" "
					//hh=%07"XINT32" ch=%08"XINT32" "
					"size=%07"INT32" "
					"ch32=%010"UINT32" "
					"clen=%07"INT32" "
					"cs=%04d "
					"lang=%02d "
					"sni=%03"INT32" "
					//"cats=%"INT32" "
					"lastspidered=%s "
					"ip=%s "
					"numLinkTexts=%04"INT32" "
					"%s"
					"version=%02"INT32" "
					//"maxLinkTextWeight=%06"UINT32"%% "
					"hc=%"INT32" "
					"redir=%s "
					"url=%s "
					"firstdup=1 "
					"shard=%"INT32" "
					"\n", 
					k.n1 , k.n0 , 
					//rec[0] , 
					docId ,
					//hostHash ,
					//contentHash ,
					recSize - 16 ,
					xd->m_contentHash32,
					xd->size_utf8Content,//tr.getContentLen
					xd->m_charset,//tr.getCharset(),
					xd->m_langId,//tr.getLanguage(),
					(int32_t)xd->m_siteNumInlinks,//tr.getDo
					//nc,
					ppp, 
					iptoa(xd->m_ip),//ipbuf , 
					info->getNumGoodInlinks(),
					foo,
					(int32_t)xd->m_version,
					//ms,
					(int32_t)xd->m_hopCount,
					ru,
					u->getUrl() ,
					shard );
				prevId = docId;
				count = 0;
				continue;
			}
			// print previous docid that is same as our
			if ( count++ == 0 ) printf ( "\n%s" , ttt );
		}
		// nice, this is never 0 for a titlerec, so we can use 0 to signal
		// that the following bytes are not compressed, and we can store
		// out special checksum vector there for fuzzy deduping.
		//if ( rec[0] != 0 ) continue;
		// print it out
		//printf("n1=%08"XINT32" n0=%016"XINT64" b=0x%02hhx docId=%012"INT64" sh=%07"XINT32" ch=%08"XINT32" "
		// date indexed as local time, not GMT/UTC
		time_t ts = xd->m_spideredTime;//tr.getSpiderDate();
		struct tm *timeStruct = localtime ( &ts );
		//struct tm *timeStruct = gmtime ( &ts );
		char ppp[100];
		strftime(ppp,100,"%b-%d-%Y-%H:%M:%S",timeStruct);
		//ppp[strlen(ppp)-2]='\0';

		/*
		  BEGIN MOD FOR DUMPING STUFF TO RE-LINK ANALYZE

		LinkInfo *info = tr.getLinkInfo();
		int32_t nLinkTexts = info->getNumLinkTexts();
		if ( nLinkTexts > 10 ) continue;
		// continue if spidered after june 14
		if ( timeStruct->tm_mon == 5 && // june
		     timeStruct->tm_mday >= 14 ) continue;
		// get sum of them link texts
		int32_t sum = 0;
		char *pp = NULL;
		int32_t nexti = 0;
		uint32_t baseScore = 0;
		for ( int32_t i = 0 ; i < nLinkTexts ; i++ ) {
			info->getLinkText ( 0 ,
					    NULL , // len
					    NULL , // itemPtr
					    NULL ,  // itemLen
					    &baseScore ,
					    NULL , // quality
					    NULL , // numLinks
					    NULL , // docId
					    NULL , // ip
					    &nexti , // nexti
					    &pp  );// nextp
			sum += baseScore;
		}
		// skip if not very high scoring
		// *100/256 to get the percentages seen in PageTitledb.cpp
		if ( sum < 10000 ) continue;
		// print it
		log ( LOG_INFO, "%s %"INT32" links sum %"INT32"", 
		      tr.getUrl()->getUrl(), nLinkTexts , sum );
		// continue
		continue;
		*/
		//uint32_t ms = 0;
		LinkInfo *info = xd->ptr_linkInfo1;//tr.getLinkInfo();
		//for ( Inlink*k=NULL;info&&(k=info->getNextInlink(k)); ){
		//	// returns NULL if none
		//	if ( k->m_baseScore > (int32_t)ms ) ms = k->m_baseScore;
		//}
		// normalize
		//ms = ((int64_t)ms * 100LL) / 256LL;

		char foo[1024];
		foo[0] = '\0';
		//if ( tr.getVersion() >= 86 ) 
		sprintf(foo,
			//"tw=%"INT32" hw=%"INT32" upw=%"INT32" "
			"sni=%"INT32" ",
			//(int32_t)xd->m_titleWeight,
			//(int32_t)xd->m_headerWeight,
			//(int32_t)xd->m_urlPathWeight,
			(int32_t)xd->m_siteNumInlinks);

		char *ru = xd->ptr_redirUrl;
		if ( ! ru ) ru = "";

		fprintf(stdout,
			"n1=%08"XINT32" n0=%016"XINT64" docId=%012"INT64" "
			//hh=%07"XINT32" ch=%08"XINT32" "
			//"e=%02"XINT32" "
			"size=%07"INT32" "
			"ch32=%010"UINT32" "
			"clen=%07"INT32" "
			"cs=%04d "
			"ctype=%s "
			"lang=%02d "
			"sni=%03"INT32" "
			//"cats=%"INT32" "
			"lastspidered=%s "
			"ip=%s "
			"numLinkTexts=%04"INT32" "
			"%s"
			"version=%02"INT32" "
			//"maxLinkTextWeight=%06"UINT32"%% "
			"hc=%"INT32" "
			"shard=%"INT32" "
			"metadatasize=%"INT32" "
			//"diffbot=%"INT32" "
			"redir=%s "
			"url=%s\n", 
			k.n1 , k.n0 , 
			//rec[0] , 
			docId ,
			//hostHash ,
			//contentHash ,
			//(int32_t)ext ,
			recSize - 16 ,
			xd->m_contentHash32,
			xd->size_utf8Content,//tr.getContentLen() ,
			xd->m_charset,//tr.getCharset(),
			g_contentTypeStrings[xd->m_contentType],
			xd->m_langId,//tr.getLanguage(),
			(int32_t)xd->m_siteNumInlinks,//tr.getDocQuality(),
			//nc,
			ppp, 
			iptoa(xd->m_ip),//ipbuf , 
			info->getNumGoodInlinks(),
			foo,
			(int32_t)xd->m_version,
			//ms,
			(int32_t)xd->m_hopCount,
			shard,
			0,
			//(int32_t)xd->m_isDiffbotJSONObject,
			ru,
			u->getUrl() );
		//printf("%s\n",xd->ptr_utf8Content);
		// free the mem
		xd->reset();
		//g_mem.printMem();
	}
	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key_t *)list.getLastKey() ) return;
	goto loop;
}

void dumpWaitingTree (char *coll ) {
	RdbTree wt;
	if (!wt.set(0,-1,true,20000000,true,"waittree2",
		    false,"waitingtree",sizeof(key_t)))return;
	collnum_t collnum = g_collectiondb.getCollnum ( coll );
	// make dir
	char dir[500];
	sprintf(dir,"%scoll.%s.%"INT32"",g_hostdb.m_dir,coll,(int32_t)collnum);
	// load in the waiting tree, IPs waiting to get into doledb
	BigFile file;
	file.set ( dir , "waitingtree-saved.dat" , NULL );
	bool treeExists = file.doesExist() > 0;
	// load the table with file named "THISDIR/saved"
	RdbMem wm;
	if ( treeExists && ! wt.fastLoad(&file,&wm) ) return;
	// the the waiting tree
	int32_t node = wt.getFirstNode();
	for ( ; node >= 0 ; node = wt.getNextNode(node) ) {
		// breathe
		QUICKPOLL(MAX_NICENESS);
		// get key
		key_t *key = (key_t *)wt.getKey(node);
		// get ip from that
		int32_t firstIp = (key->n0) & 0xffffffff;
		// get the time
		uint64_t spiderTimeMS = key->n1;
		// shift upp
		spiderTimeMS <<= 32;
		// or in
		spiderTimeMS |= (key->n0 >> 32);
		// get the rest of the data
		fprintf(stdout,"time=%"UINT64" firstip=%s\n",
			spiderTimeMS,
			iptoa(firstIp));
	}
}


void dumpDoledb (char *coll,int32_t startFileNum,int32_t numFiles,bool includeTree){
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	//g_conf.m_spiderdbMaxDiskPageCacheMem   = 0;
	//g_conf.m_doledbMaxDiskPageCacheMem = 0;
	g_doledb.init ();
	//g_collectiondb.init(true);
	g_doledb.getRdb()->addRdbBase1(coll );
	key_t startKey ;
	key_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;
	Msg5 msg5;
	RdbList list;
	key_t oldk; oldk.setMin();
	CollectionRec *cr = g_collectiondb.getRec(coll);
 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_DOLEDB    ,
			      cr->m_collnum          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k    = list.getCurrentKey();
		if ( oldk > k ) 
			fprintf(stdout,"got bad key order. "
				"%"XINT32"/%"XINT64" > %"XINT32"/%"XINT64"\n",
				oldk.n1,oldk.n0,k.n1,k.n0);
		oldk = k;
		// get it
		char *drec = list.getCurrentRec();
		// sanity check
		if ( (drec[0] & 0x01) == 0x00 ) {char *xx=NULL;*xx=0; }
		// get spider rec in it
		char *srec = drec + 12 + 4;
		// print doledb info first then spider request
		fprintf(stdout,"dolekey=%s (n1=%"UINT32" n0=%"UINT64") "
			"pri=%"INT32" "
			"spidertime=%"UINT32" "
			"uh48=0x%"XINT64"\n",
			KEYSTR(&k,12),
			k.n1,
			k.n0,
			(int32_t)g_doledb.getPriority(&k),
			g_doledb.getSpiderTime(&k),
			g_doledb.getUrlHash48(&k));
		fprintf(stdout,"spiderkey=");
		// print it
		g_spiderdb.print ( srec );
		// the \n
		printf("\n");
		// must be a request -- for now, for stats
		if ( ! g_spiderdb.isSpiderRequest((key128_t *)srec) ) {
			// error!
			continue;
			char *xx=NULL;*xx=0; }
		// cast it
		SpiderRequest *sreq = (SpiderRequest *)srec;
		// skip negatives
		if ( (sreq->m_key.n0 & 0x01) == 0x00 ) {
			char *xx=NULL;*xx=0; }
	}
	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key_t *)list.getLastKey() ) return;
	goto loop;
}


// . dataSlot fo the hashtable for spider stats in dumpSpiderdb
// . key is firstip
class UStat {
public:
	// for spider requests:
	int32_t m_numRequests;
	int32_t m_numRequestsWithReplies;
	int32_t m_numWWWRoots;
	int32_t m_numNonWWWRoots;
	int32_t m_numHops1;
	int32_t m_numHops2;
	int32_t m_numHops3orMore;
	int32_t m_ageOfYoungestSpideredRequest;
	int32_t m_ageOfOldestUnspideredRequest;
	int32_t m_ageOfOldestUnspideredWWWRootRequest;
	// for spider replies:
	int32_t m_numGoodReplies;
	int32_t m_numErrorReplies;
};

static HashTableX g_ut;

void addUStat1 ( SpiderRequest *sreq, bool hadReply , int32_t now ) {
	int32_t firstIp = sreq->m_firstIp;
	// lookup
	int32_t n = g_ut.getSlot ( &firstIp );
	UStat *us = NULL;
	UStat tmp;
	if ( n < 0 ) {
		us = &tmp;
		memset(us,0,sizeof(UStat));
		g_ut.addKey(&firstIp,us);
		us = (UStat *)g_ut.getValue ( &firstIp );
	}
	else {
		us = (UStat *)g_ut.getValueFromSlot ( n );
	}
	int32_t age = now - sreq->m_addedTime;
	// inc the counts
	us->m_numRequests++;
	if ( hadReply) us->m_numRequestsWithReplies++;
	if ( sreq->m_hopCount == 0 ) {
		if  ( sreq->m_isWWWSubdomain ) us->m_numWWWRoots++;
		else                           us->m_numNonWWWRoots++;
	}
	else if ( sreq->m_hopCount == 1 ) us->m_numHops1++;
	else if ( sreq->m_hopCount == 2 ) us->m_numHops2++;
	else if ( sreq->m_hopCount >= 3 ) us->m_numHops3orMore++;
	if ( hadReply ) {
		if (age < us->m_ageOfYoungestSpideredRequest ||
		          us->m_ageOfYoungestSpideredRequest == 0 )
			us->m_ageOfYoungestSpideredRequest = age;
	}
	if ( ! hadReply ) {
		if (age > us->m_ageOfOldestUnspideredRequest ||
		          us->m_ageOfOldestUnspideredRequest == 0 )
			us->m_ageOfOldestUnspideredRequest = age;
	}
	if ( ! hadReply && sreq->m_hopCount == 0 && sreq->m_isWWWSubdomain ) {
		if (age > us->m_ageOfOldestUnspideredWWWRootRequest ||
		          us->m_ageOfOldestUnspideredWWWRootRequest == 0 )
			us->m_ageOfOldestUnspideredWWWRootRequest = age;
	}
}

void addUStat2 ( SpiderReply *srep , int32_t now ) {
	int32_t firstIp = srep->m_firstIp;
	// lookup
	int32_t n = g_ut.getSlot ( &firstIp );
	UStat *us = NULL;
	UStat tmp;
	if ( n < 0 ) {
		us = &tmp;
		memset(us,0,sizeof(UStat));
		g_ut.addKey(&firstIp,us);
		us = (UStat *)g_ut.getValue ( &firstIp );
	}
	else {
		us = (UStat *)g_ut.getValueFromSlot ( n );
	}
	//int32_t age = now - srep->m_spideredTime;
	// inc the counts
	if ( srep->m_errCode )
		us->m_numErrorReplies++;
	else
		us->m_numGoodReplies++;

}


int32_t dumpSpiderdb ( char *coll,
		    int32_t startFileNum , int32_t numFiles , bool includeTree ,
		    char printStats ,
		    int32_t firstIp ) {
	if ( startFileNum < 0 ) {
		log(LOG_LOGIC,"db: Start file number is < 0. Must be >= 0.");
		return -1;
	}		

	if ( printStats == 1 ) {
		//g_conf.m_maxMem = 2000000000LL; // 2G
		//g_mem.m_maxMem  = 2000000000LL; // 2G
		if ( ! g_ut.set ( 4, sizeof(UStat), 10000000, NULL,
				  0,0,false,"utttt") )
			return -1;
	}

	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	//g_conf.m_spiderdbMaxDiskPageCacheMem   = 0;
	g_spiderdb.init ();
	//g_collectiondb.init(true);
	g_spiderdb.getRdb()->addRdbBase1(coll );
	key128_t startKey ;
	key128_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	// start based on firstip if non-zero
	if ( firstIp ) {
		startKey = g_spiderdb.makeFirstKey ( firstIp );
		endKey  = g_spiderdb.makeLastKey ( firstIp );
	}
	//int32_t t1 = 0;
	//int32_t t2 = 0x7fffffff;
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;
	Msg5 msg5;
	RdbList list;
	// clear before calling Msg5
	g_errno = 0;

	// init stats vars
	int32_t negRecs   = 0;
	int32_t emptyRecs = 0;
	int32_t uniqDoms  = 0;
	// count urls per domain in "domTable"
	HashTable domTable;
	domTable.set ( 1024*1024 );
	// count every uniq domain per ip in ipDomTable (uses dup keys)
	HashTableX ipDomTable;
	// allow dups? true!
	ipDomTable.set ( 4,4,5000000 , NULL, 0, true ,0, "ipdomtbl");
	// count how many unique domains per ip
	HashTable ipDomCntTable;
	ipDomCntTable.set ( 1024*1024 );
	// buffer for holding the domains
	int32_t  bufSize = 1024*1024;
	char *buf     = (char *)mmalloc(bufSize,"spiderstats");
	int32_t  bufOff  = 0;
	int32_t  count   = 0;
	int32_t  countReplies = 0;
	int32_t  countRequests = 0;
	int64_t offset = 0LL;
	int32_t now;
	static int64_t s_lastRepUh48 = 0LL;
	static int32_t s_lastErrCode = 0;
	static int32_t s_lastErrCount = 0;
	CollectionRec *cr = g_collectiondb.getRec(coll);

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_SPIDERDB  ,
			      cr->m_collnum       ,
			      &list         ,
			      (char *)&startKey      ,
			      (char *)&endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return -1;
	}
	// all done if empty
	if ( list.isEmpty() ) goto done;

	// this may not be in sync with host #0!!!
	now = getTimeLocal();

	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {

		// get it
		char *srec = list.getCurrentRec();

		// save it
		int64_t curOff = offset;
		// and advance
		offset += list.getCurrentRecSize();

		// must be a request -- for now, for stats
		if ( ! g_spiderdb.isSpiderRequest((key128_t *)srec) ) {
			// print it
			if ( ! printStats ) {
				printf( "offset=%"INT64" ",curOff);
				g_spiderdb.print ( srec );
				printf("\n");
			}
			// its a spider reply
			SpiderReply *srep = (SpiderReply *)srec;
			// store it
			s_lastRepUh48 = srep->getUrlHash48();
			s_lastErrCode = srep->m_errCode;
			s_lastErrCount = srep->m_errCount;
			countReplies++;
			// get firstip
			if ( printStats == 1 ) addUStat2 ( srep , now );
			continue;
		}

		// cast it
		SpiderRequest *sreq = (SpiderRequest *)srec;

		countRequests++;

		int64_t uh48 = sreq->getUrlHash48();
		// count how many requests had replies and how many did not
		bool hadReply = ( uh48 == s_lastRepUh48 );

		// get firstip
		if ( printStats == 1 ) addUStat1 ( sreq , hadReply , now );

		// print it
		if ( ! printStats ) {
			printf( "offset=%"INT64" ",curOff);
			g_spiderdb.print ( srec );

			printf(" requestage=%"INT32"s",now-sreq->m_addedTime);
			printf(" hadReply=%"INT32"",(int32_t)hadReply);

			printf(" errcount=%"INT32"",(int32_t)s_lastErrCount);

			if ( s_lastErrCode )
				printf(" errcode=%"INT32"(%s)",(int32_t)s_lastErrCode,
				       mstrerror(s_lastErrCode));
			else
				printf(" errcode=%"INT32"",(int32_t)s_lastErrCode);

			// we haven't loaded hosts.conf so g_hostdb.m_map
			// is not set right... so this is useless
			//printf(" shard=%"INT32"\n",
			//     (int32_t)g_hostdb.getShardNum(RDB_SPIDERDB,sreq));
			printf("\n");
		}

		// print a counter
		if ( ((count++) % 100000) == 0 ) 
			fprintf(stderr,"Processed %"INT32" records.\n",count-1);

		if ( printStats != 2 ) continue;

		// skip negatives
		if ( (sreq->m_key.n0 & 0x01) == 0x00 ) continue;

		// skip bogus shit
		if ( sreq->m_firstIp == 0 || sreq->m_firstIp==-1 ) continue;

		// shortcut
		int32_t domHash = sreq->m_domHash32;
		// . is it in the domain table?
		// . keeps count of how many urls per domain
		int32_t slot = domTable.getSlot ( domHash );
		if ( slot >= 0 ) {
			int32_t off = domTable.getValueFromSlot ( slot );
			// just inc the count for this domain
			*(int32_t *)(buf + off) = *(int32_t *)(buf + off) + 1;
			continue;
		}

		// get the domain
		int32_t  domLen = 0;
		char *dom = getDomFast ( sreq->m_url , &domLen );

		// always need enough room...
		if ( bufOff + 4 + domLen + 1 >= bufSize ) {
			int32_t  growth     = bufSize * 2 - bufSize;
			// limit growth to 10MB each time
			if ( growth > 10*1024*1024 ) growth = 10*1024*1024;
			int32_t  newBufSize = bufSize + growth;
			char *newBuf = (char *)mrealloc( buf , bufSize , 
							 newBufSize,
							 "spiderstats");
			if ( ! newBuf ) return -1;
			// re-assign
			buf     = newBuf;
			bufSize = newBufSize;
		}

		// otherwise add it, it is a new never-before-seen domain
		//char poo[999];
		//gbmemcpy ( poo , dom , domLen );
		//poo[domLen]=0;
		//fprintf(stderr,"new dom %s hash=%"INT32"\n",dom,domHash);
		// store the count of urls followed by the domain
		char *ptr = buf + bufOff;
		*(int32_t *)ptr = 1;
		ptr += 4;
		gbmemcpy ( ptr , dom , domLen );
		ptr += domLen;
		*ptr = '\0';
		// use an ip of 1 if it is 0 so it hashes right
		int32_t useip = sreq->m_firstIp; // ip;
		// can't use 1 because it all clumps up!!
		//if ( ip == 0 ) useip = domHash ;
		// this table counts how many urls per domain, as
		// well as stores the domain
		if ( ! domTable.addKey (domHash , bufOff) ) return -1;
		// . if this is the first time we've seen this domain,
		//   add it to the ipDomTable
		// . this hash table must support dups.
		// . we need to print out all the domains for each ip
		if ( ! ipDomTable.addKey ( &useip , &bufOff ) ) return -1;
		// . this table counts how many unique domains per ip
		// . it is kind of redundant since we have ipDomTable
		int32_t ipCnt = ipDomCntTable.getValue ( useip );
		if ( ipCnt < 0 ) ipCnt = 0;
		if ( ! ipDomCntTable.addKey ( useip, ipCnt+1) ) return -1;
		// advance to next empty spot
		bufOff += 4 + domLen + 1;
		// count unque domains
		uniqDoms++;
	}

	startKey = *(key128_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey >= *(key128_t *)list.getLastKey() ) goto loop;

 done:
	// print out the stats
	if ( ! printStats ) return 0;


	// print UStats now
	if ( printStats == 1 ) {
		for ( int32_t i = 0 ; i < g_ut.getNumSlots();i++ ) {
			if ( g_ut.m_flags[i] == 0 ) continue;
			UStat *us = (UStat *)g_ut.getValueFromSlot(i);
			int32_t firstIp = *(int32_t *)g_ut.getKeyFromSlot(i);
			fprintf(stdout,"%s ",
				iptoa(firstIp));
			fprintf(stdout,"requests=%"INT32" ",
				us->m_numRequests);
			fprintf(stdout,"wwwroots=%"INT32" ",
				us->m_numWWWRoots);
			fprintf(stdout,"nonwwwroots=%"INT32" ",
				us->m_numNonWWWRoots);
			fprintf(stdout,"1hop=%"INT32" ",
				us->m_numHops1);
			fprintf(stdout,"2hop=%"INT32" ",
				us->m_numHops2);
			fprintf(stdout,"3hop+=%"INT32" ",
				us->m_numHops3orMore);
			fprintf(stdout,"mostrecentspider=%"INT32"s ",
				us->m_ageOfYoungestSpideredRequest);
			fprintf(stdout,"oldestunspidered=%"INT32"s ",
				us->m_ageOfOldestUnspideredRequest);
			fprintf(stdout,"oldestunspideredwwwroot=%"INT32" ",
				us->m_ageOfOldestUnspideredWWWRootRequest);
			fprintf(stdout,"spidered=%"INT32" ",
				us->m_numRequestsWithReplies);
			fprintf(stdout,"goodspiders=%"INT32" ",
				us->m_numGoodReplies);
			fprintf(stdout,"errorspiders=%"INT32"",
				us->m_numErrorReplies);
			fprintf(stdout,"\n");
		}
		return 0;
	}


	int32_t uniqIps = ipDomCntTable.getNumSlotsUsed();

	// print out all ips, and # of domains they have and list of their
	// domains
	int32_t nn = ipDomTable.getNumSlots();
	// i is the bucket to start at, must be EMPTY!
	int32_t i = 0;
	// count how many buckets we visit
	int32_t visited = 0;
	// find the empty bucket
	for ( i = 0 ; i < nn ; i++ )
		if ( ipDomTable.m_flags[i] == 0 ) break;
		//if ( ipDomTable.getKey(i) == 0 ) break;
	// now we can do our scan of the ips. there can be dup ips in the
	// table so we must chain for each one we find
	for ( ; visited++ < nn ; i++ ) {
		// wrap it
		if ( i == nn ) i = 0;
		// skip empty buckets
		if ( ipDomTable.m_flags[i] == 0 ) continue;
		// get ip of the ith slot
		int32_t ip = *(int32_t *)ipDomTable.getKeyFromSlot(i);
		// get it in the ip table, if not there, skip it
		int32_t domCount = ipDomCntTable.getValue ( ip ) ;
		if ( domCount == 0 ) continue;
		// log the count
		int32_t useip = ip;
		if ( ip == 1 ) useip = 0;
		fprintf(stderr,"%s has %"INT32" domains.\n",iptoa(useip),domCount);
		// . how many domains on that ip, print em out
		// . use j for the inner loop
		int32_t j = i;
		// buf for printing ip
		char ipbuf[64];
		sprintf (ipbuf,"%s",iptoa(useip) );
	jloop:
		int32_t ip2 = *(int32_t *)ipDomTable.getKeyFromSlot ( j ) ;
		if ( ip2 == ip ) {
			// get count
			int32_t  off = *(int32_t *)ipDomTable.getValueFromSlot ( j );
			char *ptr = buf + off;
			int32_t  cnt = *(int32_t *)ptr;
			char *dom = buf + off + 4;
			// print: "IP Domain urlCountInDomain"
			fprintf(stderr,"%s %s %"INT32"\n",ipbuf,dom,cnt);
			// advance && wrap
			if ( ++j >= nn ) j = 0;
			// keep going
			goto jloop;
		}
		// not an empty bucket, so keep chaining
		if ( ip2 != 0 ) { 
			// advance & wrap
			if ( ++j >= nn ) j = 0; 
			// keep going
			goto jloop; 
		}
		// ok, we are done, do not do this ip any more
		ipDomCntTable.removeKey(ip);
	}

	if ( negRecs )
		fprintf(stderr,"There are %"INT32" total negative records.\n",
			negRecs);
	if ( emptyRecs ) 
		fprintf(stderr,"There are %"INT32" total negative records.\n",
			emptyRecs);

	//fprintf(stderr,"There are %"INT32" total urls.\n",count);
	fprintf(stderr,"There are %"INT32" total records.\n",count);
	fprintf(stderr,"There are %"INT32" total request records.\n",countRequests);
	fprintf(stderr,"There are %"INT32" total replies records.\n",countReplies);
	// end with total uniq domains
	fprintf(stderr,"There are %"INT32" unique domains.\n",uniqDoms);
	// and with total uniq ips in this priority
	fprintf(stderr,"There are %"INT32" unique IPs.\n",uniqIps);
	return 0;
}

// . for cleaning up indexdb
// . print out docids in indexdb but not in our titledb, if they should be
void dumpMissing ( char *coll ) {
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	//g_conf.m_indexdbMaxCacheMem = 0;
	//g_conf.m_clusterdbMaxDiskPageCacheMem = 0;

	g_collectiondb.loadAllCollRecs();
	g_titledb.init();
	g_titledb.getRdb()->addRdbBase1 ( coll );
	// if titledb has stuff in memory, do not do this, it needs to
	// be dumped out. this way we can assume a tfn of 255 means the docid
	// is probable and just in spiderdb. (see loop below)
	if ( g_titledb.getRdb()->m_tree.getNumUsedNodes() ) {
		logf(LOG_INFO,"db: Titledb needs to be dumped to disk before "
		     "we can scan tfndb. Please do ./gb ddump to do this or "
		     "click on \"dump to disk\" in the Master Controls.");
		return;
	}

	// MDW FIX THIS RIGHT!
	int64_t numRecs = 12345;//g_tfndb.getRdb()->getNumTotalRecs();
	int64_t oldNumSlots = (numRecs * 100) / 80;
	// make a power of 2
	// make it a power of 2
	//oldNumSlots *= 2;
	//oldNumSlots -= 1;
	//int32_t numSlots = getHighestLitBitValue((uint32_t)oldNumSlots);
	int32_t numSlots = oldNumSlots;
	//uint32_t mask = numSlots - 1;
	
	// make a hash table for docids
	logf(LOG_INFO,"db: Allocating %"INT32" bytes for docids.",numSlots*8);
	uint64_t *slots = 
		(uint64_t *)mcalloc ( numSlots * 8 , "dumpMissing" );
	if ( ! slots ) {
		log("db: Could not alloc %"INT32" bytes to load in %"INT64" docids.",
		    numSlots*8,numRecs);
		return;
	}

	// load in all tfndb recs
	key_t startKey ;
	key_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 5*1024*1024;
	RdbList list;
	Msg5 msg5;

	logf(LOG_INFO,"db: Loading tfndb for hostId %"INT32", has %"INT64" recs.",
	     g_hostdb.m_hostId,numRecs);
	int64_t count = 0;
	int32_t next = 0;
	int32_t used = 0;
	CollectionRec *cr = g_collectiondb.getRec(coll);
 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_TFNDB     ,
			      cr->m_collnum    ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		exit(-1);
	}
	// all done if empty
	if ( list.isEmpty() ) goto done;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		// get the tfn
		key_t k   = list.getCurrentKey();
		count++;
		// skip if negative
		if ( (k.n0 & 0x01LL) == 0x00 ) continue;
		// titledb tree is empty, so this must indicate it is in
		// spiderdb only
		int32_t  tfn = 0;//g_tfndb.getTfn(&k);
		if ( tfn == 255 ) continue;
		// get docid
		uint64_t d = 0LL;//g_tfndb.getDocId ( &k );
		// add to hash table
		//int32_t n = (uint32_t)d & mask;
		int32_t n = (uint32_t)d % numSlots;
		// chain if not in there
		while ( slots[n] ) 
			if ( ++n >= numSlots ) n = 0;
		// add it here
		slots[n] = d;
		// count it
		if ( used >= next ) {
			logf(LOG_INFO,"db: Loaded %"INT32" docids.",used);
			next = used + 1000000;
		}
		used++;
	}
	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey >= *(key_t *)list.getLastKey() ) goto loop;


	// ok now, scan indexdb and report docids in indexdb that are not
	// in our tfndb when they should be.

 done:
	logf(LOG_INFO,"db: Scanned %"INT64" tfndb recs.",count);
	logf(LOG_INFO,"db: Scanning indexdb.");
	logf(LOG_INFO,"db: Tight merge indexdb to make this faster.");

	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	g_indexdb.init ();
	//g_collectiondb.init(true);
	g_indexdb.getRdb()->addRdbBase1 ( coll );
	startKey.setMin();
	endKey.setMax();
	// get a meg at a time
	minRecSizes = 5*1024*1024;

	//uint32_t groupId = g_hostdb.m_groupId;
	uint32_t shardNum = g_hostdb.getMyShardNum();
	count = 0;
	int32_t scanned = 0;
	//HashTableT <int64_t,char> repeat;
	HashTableX repeat;
	if ( ! repeat.set ( 8,1,1000000,NULL,0,false,0,"rpttbl" ) ) {
		log("db: Failed to init repeat hash table.");
		return;
	}

 loop2:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_INDEXDB   ,
			      cr->m_collnum       ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// something to log
	scanned += list.getListSize();
	if ( scanned >= 100000000 ) {
		count += scanned;
	scanned = 0;
		logf(LOG_INFO,"db: Scanned %"INT64" bytes.",count);
	}
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {

		key_t k    = list.getCurrentKey();
		// skip deletes
		if ( (k.n0 & 0x01) == 0x00 ) continue;
		// do we hold his titleRec? continue if not
		if ( getShardNum ( RDB_TITLEDB , &k ) != shardNum ) continue;
		// get his docid
		uint64_t d = g_indexdb.getDocId(k);
		// otherwise, report him if not in tfndb
		int32_t n = (uint32_t)d % numSlots;
		while ( slots[n] && slots[n] != d )
			if ( ++n >= numSlots ) n = 0;
		// if he was not in tfndb when he should have been,
		// print him on stdout
		if ( slots[n] == d ) continue;
		// is he in the repeat table?
		int32_t slot=repeat.getSlot(&d);
		if (slot!=-1)
			if ( *(char *)repeat.getValueFromSlot ( slot ) == 1 ) 
				continue;
		// print if this is the first time
		fprintf(stderr,"missingdocid %012"UINT64"\n",d);
		// put him in a table so we don't repeat him
		char one = 1;
		repeat.addKey ( &d , &one );
	}

	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey >= *(key_t *)list.getLastKey() ) goto loop2;
	logf(LOG_INFO,"db: Done generating missing docids.");
	return;
}

// . for cleaning up indexdb
// . print out docids in the same termlist multiple times
void dumpDups ( char *coll ) {
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	//g_conf.m_indexdbMaxCacheMem = 0;

	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	g_indexdb.init ();
	//g_collectiondb.init(true);
	g_indexdb.getRdb()->addRdbBase1 ( coll );

	key_t startKey ;
	key_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 6*1024*1024;
	int32_t numSlots = 2 * 1024 * 1024;
	int64_t * slots;
	char * scores;
	slots = (int64_t *) mmalloc ( numSlots * 8, "main-dumpDups");
	scores = (char *) mmalloc ( numSlots, "main-dumpDups");
	if(!slots || !scores) {
		if(!slots)
			log(LOG_INFO,"admin: Could not allocate %"INT64" "
				   "bytes for dumpDups" , 
				   (int64_t) numSlots * 8 );
		else mfree(slots, numSlots * 8, "main-dumpDups" );

		if(!scores)
			log(LOG_INFO,"admin: Could not allocate %"INT32" "
				   "bytes for dumpDups" , 
				    numSlots  );
		else mfree(scores, numSlots , "main-dumpDups" );
		return; 
	}

	int32_t offset1 = 0;
	int32_t offset2 = 0;
	int64_t tempTid = -1;
	int64_t lastTid = -1;
	int64_t tid = -1;

	int64_t indexdbCount = 0;
	char * tempListPtr;
	char * tempListPtrHi;
	key_t k;
	int64_t d;
	int32_t hashMod;
	uint64_t n2;
	int64_t endTid;
	char filename[30];
	char buff[100];
	int32_t numParsed = 0;
	int32_t collNum = g_collectiondb.getCollnum ( coll );
	File f;
	File f2;
	Rdb *r = g_indexdb.getRdb();
	RdbTree *tree = &r->m_tree;

	sprintf(filename,"removedDupKeys.%"INT32"", collNum );
	f.set(g_hostdb.m_dir, filename);
	if(f.doesExist() ) {
		log(LOG_INFO,"admin: File %s%s already exists. "
		    "Aborting process" , g_hostdb.m_dir, filename );
		return;
	}
	if( !f.open(O_RDWR | O_CREAT) ) {
		log( LOG_INFO, "admin: Could not create %s/%s.",
		     g_hostdb.m_dir, filename);
		return ;
	}

	sprintf(filename,"removedDupDocIds.%"INT32"", collNum);
	//	g_collectiondb.getCollnum ( coll ));
	f2.set(g_hostdb.m_dir, filename);
	if(f2.doesExist() ) {
		f2.unlink();
	}
	if( !f2.open(O_RDWR | O_CREAT) ) {
		log( LOG_INFO, "admin: Could not create %s/%s.",
		     g_hostdb.m_dir, filename);
		return ;
	}

	RdbList list;
	Msg5 msg5;

	uint32_t count = 0;
	uint32_t count2 = 0;
	int64_t byteCount = 0;
	uint32_t highLitBit;
	char *p;
	//uint32_t groupId = g_hostdb.m_groupId;
	count = 0;
	int64_t scanned = 0;

	int64_t dups = 0LL;
	char lookup[256] = { 8, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
			     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
			     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
			     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
			     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
			     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
			     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
			     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
			     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
	//	HashTableT <int64_t,char> repeat;
	//	HashTableT <int64_t,char> local;


	logf(LOG_INFO,"db: Scanning indexdb for repeated docids.");
	logf(LOG_INFO,"db: Tight merge indexdb to make this faster.");
	logf(LOG_INFO,"db: Dumping docid termId pairs to file.");

	/*	if ( ! repeat.set ( 1000000 ) ) {
		log("db: Failed to init repeat hash table.");
		return;
	}

	if ( ! local.set ( 1000000 ) ) {
		log("db: Failed to init repeat hash table2.");
		return;
	}
	*/
	CollectionRec *cr = g_collectiondb.getRec(coll);

 loop:
	//int64_t startTime = gettimeofdayInMilliseconds();
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_INDEXDB   ,
			      cr->m_collnum          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	//int64_t endTime = gettimeofdayInMilliseconds();
	//log(LOG_INFO,"dumpdups Msg5 time = %"INT32"",(int32_t)endTime - startTime);
	// all done if empty
	if ( list.isEmpty() ) {
		mfree ( slots, numSlots * 8, "main-dumpDups");
		mfree ( scores, numSlots, "main-dumpDups");
		f.close();
		f2.close();
		return;
	}

	// something to log
	scanned += list.getListSize();
		if ( scanned >= 10000000 ) 
		{
		byteCount += scanned;
		scanned = 0;
		logf(LOG_INFO,"db: Scanned %"INT64" bytes. Parsed %"INT64" records"
		     "dups=%"INT64". ",byteCount,indexdbCount, dups);
	}
	tid = -1;
	
	k = *(key_t *) list.getStartKey();
	tempTid = g_indexdb.getTermId( k );
	k = *(key_t *) list.getEndKey();
	endTid = g_indexdb.getTermId( k );
	// loop over entries in list

	memset(slots , 0, numSlots * 8);
	memset(scores, 0, numSlots);
	offset1 = 0;
	offset2 = 0;
	//startTime = gettimeofdayInMilliseconds();
	// loop over entries in list
	//startTime = gettimeofdayInMilliseconds();
	//int32_t totalNumParsed = 0;
	bool sameTidList = 0;
	int32_t thisDup = 0;
	if(tempTid == endTid) sameTidList = 1;
	//log(LOG_INFO,"sameTidList = %d",sameTidList);
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		
		k = list.getCurrentKey();
		if ( (k.n0 & 0x01LL) == 0x00 ) continue;
		tempTid = g_indexdb.getTermId(k);		
		d = g_indexdb.getDocId(k);
		//totalNumParsed++;
		numParsed++;
		//change in tid, get the count

		if(tempTid != tid ) {
			thisDup = 0;
			//is this tid the same as we process in the last run
			if(tid == -1 && tempTid == lastTid) {
				log(LOG_INFO,"admin: We broke termlist of "
				    "termid=%"INT64". Some "
			   "docids may be repeated in this termlist and "
			    "we will not know.",
			    tempTid);
			}

			//check if we hit the endTid - then reload the 
			// list from that point
			if(tempTid == endTid && !sameTidList) {
				break;
			} 
			if(sameTidList) {
				
				count = numSlots - 1;
				offset1 = 0;
				offset2 = numSlots;	
				if(tid != -1) {
					memset(slots , 0, numSlots * 8);
					memset(scores, 0, numSlots);
				//log(LOG_INFO,"dumpDups Wish more numslots");

				}
				tid = tempTid;
				hashMod = numSlots;
				lastTid = tid;
				numParsed = 1;

			} else {
				tid = tempTid;
				
				
				tempListPtr = list.m_listPtr;
				tempListPtrHi = list.m_listPtrHi;	
				count = 1;
				for( list.skipCurrentRecord();
				     !list.isExhausted();
				     list.skipCurrentRecord() ) {
					if( *(list.m_listPtr) & 0x02 ) {
						count++;
					} 
					else {
						key_t kt = list.getCurrentKey();
						tempTid=g_indexdb.getTermId(kt);
						if(tempTid != tid)	
							break;	
						count++;
						
					}
					
				}
				list.m_listPtr = tempListPtr;
				list.m_listPtrHi = tempListPtrHi;	
				
				if(count == 1) {
					continue;
				}
				
				p = (char *) &count;
				if( count*2  > (unsigned) numSlots ) 
					count = numSlots;
				else {
					if(count <= 255) {
						highLitBit= 
						   lookup[(unsigned char) *p];
						highLitBit++;
						count2 = 1;
						count2 <<= highLitBit;
						if( count2/2 < count ) {
							count2 <<= 1;
						}
						count = count2;
					} else if (count <= 65535) {
						p++;
						highLitBit= 
						   lookup[(unsigned char) *p];
						highLitBit += 9;	
						count2 = 1;
						count2 <<= highLitBit;
						if( count2/2 < count ) {
							count2 <<= 1;
						}
						count = count2;
					} else if (count <= 16777216) {
						p += 2;
						highLitBit= 
						   lookup[(unsigned char) *p];
						highLitBit += 17;
						count2 = 1;
						count2 <<= highLitBit;
						if( count2/2 < count ) {
							count2 <<= 1;
							if(count2 > 
							   (unsigned) numSlots) 
								count2=numSlots;
						}
						count = count2;
					} else {
						p += 3;
						highLitBit= 
						   lookup[(unsigned char) *p];
						highLitBit += 25;
						count2 = 1;
						count2 <<= highLitBit;
						if( count2/2 < count ) {
							count2 <<= 1;
							if(count2 > 
							   (unsigned) numSlots) 
								count2=numSlots;
						}
						count = count2;	
					}
				}
				
				if(offset2 + count + 1 < (unsigned) numSlots ) {
					offset1 =  offset2 + 1;
					offset2 += count;
				}
				else {
					memset(slots , 0, numSlots * 8);
					memset(scores, 0, numSlots);
					offset1 = 0;
					offset2 = count;
				}
				
				hashMod = count;
				lastTid = tid;
				
				numParsed = 1;			
			}	
		}
		indexdbCount ++;

		
		n2 = (uint64_t) d & (hashMod-1);
		n2 += offset1;
		while ( slots[n2] && slots[n2] != d ) {
			if ( ++n2 >= (uint64_t) offset2 ) 
				n2 = offset1;
		}

		if( slots[n2] != d ) {
			slots[n2] = d;
			scores[n2] = (unsigned char) g_indexdb.getScore(k);
		}
		else {
			dups++;

			//add negative keys
			sprintf(buff,"%08"XINT32" %016"XINT64"\n",
				k.n1, k.n0);
			f.write(buff,gbstrlen(buff), -1);
			sprintf(buff,"%"INT64"\n",d);
			f2.write(buff, gbstrlen(buff), -1);
			
			k.n0 &= 0xfffffffffffffffeLL;
			
			if ( ! r->addRecord ( coll, k , NULL , 0 , 0) ) {
				log("admin: could not add negative key: %s",
					   mstrerror(g_errno));
				return;
			}
			
			if ( tree->getNumAvailNodes() <= 0 ) {
				// this should block
				r->dumpTree(0);
			}
			
			key_t kt;

			kt = g_indexdb.makeKey(tid,scores[n2],slots[n2],false);
			sprintf(buff,"%08"XINT32" %016"XINT64"\n",
				kt.n1, kt.n0);

			kt.n0 &= 0xfffffffffffffffeLL;
			f.write(buff,gbstrlen(buff), -1);
			
			if ( ! r->addRecord ( coll, kt , NULL , 0 , 0) ) {
				log("admin: could not add negative key: %s",
				    mstrerror(g_errno));
				return;
			}
			
			if ( tree->getNumAvailNodes() <= 0 ) {
				// this should block
				r->dumpTree(0);
			}
		}
		
		if(numParsed*2 + 1 > numSlots ) {
		//log(LOG_INFO,"dumpDups wished more numSlots numParsed=%"INT32"",
		//    numParsed);
			tid = 0;
		}
	}
	// no, use the last termid!! well this is not perfect, oh well
	//endTime = gettimeofdayInMilliseconds();
	//log(LOG_INFO,"dumpdups Loop time = %"INT64" notParsed=%"INT32"",
	//    endTime - startTime,  (list.getNumRecs()-totalNumParsed));
	if( list.isExhausted() ) {
		startKey = *(key_t *)list.getLastKey();
		startKey += (uint32_t) 1;
		if ( startKey >= *(key_t *)list.getLastKey() ) goto loop;
	}
	else {
		startKey = k;
		goto loop;
	}
	// watch out for wrap around

	logf(LOG_INFO,"db: Done generating missing docids. Parsed %"INT64""
	     " indexdb records", indexdbCount);
	mfree ( slots, numSlots * 8, "main-dumpDups");
	mfree ( scores, numSlots, "main-dumpDups");
	f.close();
	f2.close();
	list.freeList();
	r->close ( NULL , NULL , false , false );

	return;
}


// . remove the docids in "filename" from indexdb.
// . make a hashtable of these docids
// . once each host has a list of the docids in /a/missing*, do this:
// dsh -ac 'cd /a/ ; echo -n "cat /a/missing* | grep missingdoc | awk '{print $2}' | sort > sorted." > /a/poo ; cd /a/ ; ls missing* >> /a/poo ; chmod +x /a/poo; /a/poo'
// . then each host will have a file called /a/sorted.missing* and you can
//   copy them to host #0 and merge sort them with 'sort -m -t /a/tmp sorted.*'
void removeDocIds  ( char *coll , char *filename ) {

	int fd;
	fd = open ( filename , O_RDONLY );
	if ( fd <= 0 ) {
		log("db: Count not open %s for reading: %s",
		    filename,strerror(errno));
		return ;
	}

	int64_t dcount = 0;
	int64_t offset ;
	char buf [ 1024*1024*2+1 ];
	int32_t readSize ;
	int32_t n ;
	char *p;
	char *pend;

	// note it
	logf(LOG_INFO,"db: Counting docids in file %s.",filename);

 loop1:
	// read in docids and hash them
	offset = 0;
	readSize = 1024*1024*2;
	n = read ( fd , buf , readSize );
	if ( n < 0 ) {
		log("db: Had error reading %s: %s",
		    filename,strerror(errno));
		return ;
	}
	offset += n;
	// 0 is EOF
	p    = buf;
	pend = buf + n;
	*pend = 0;
	while ( *p ) {
		// count it for now
		dcount++;
		// advance over \n
		while ( *p && *p !='\n' ) p++;
		// all done?
		while ( *p == '\n' ) p++;
	}
	if ( n > 0 ) goto loop1;

	// note it
	logf(LOG_INFO,"db: Counted %"INT64" docids in file %s.",dcount,filename);

	int64_t oldNumSlots = (dcount * 100LL) / 80LL;
	oldNumSlots *= 2;
	oldNumSlots -= 1;
	int32_t numSlots = getHighestLitBitValue ((uint32_t)oldNumSlots);
	if ( numSlots < 64 ) numSlots = 64;
	int32_t need = numSlots * 8;
	logf(LOG_INFO,"db: Allocating %"INT32" bytes for hash table.",need);
	uint32_t mask = numSlots - 1;
	uint64_t *slots = 
		(uint64_t *)mcalloc(need,"loaddocids");
	if ( ! slots ) {
		log("db: Could not allocate %"INT32" bytes to read in docids. "
		    "Please split this file and do multiple runs.", need);
		return;
	}

	// now hash those docids
	offset = 0;
	close ( fd );
	fd = open ( filename , O_RDONLY );
	if ( fd <= 0 ) {
		log("db: Count not open %s for reading: %s",
		    filename,strerror(errno));
		return ;
	}

	// note it
	logf(LOG_INFO,"db: Loading and hashing docids from file %s.",filename);

 loop2:
	// read in docids and hash them
	n = read ( fd , buf , readSize );
	if ( n < 0 ) {
		log("db: Had error reading %s: %s",
		    filename,strerror(errno));
		return ;
	}
	offset += n;
	// 0 is EOF
	p    = buf;
	pend = buf + n;
	*pend = 0;
	while ( *p ) {
		// get docid
		uint64_t d = atoll(p);
		// hash it
		int32_t n = (uint32_t)d & mask;
		while ( slots[n] && slots[n] != d )
			if ( ++n >= numSlots ) n = 0;
		// add him
		slots[n] = d;
		// advance over \n
		while ( *p && *p !='\n' ) p++;
		// all done?
		while ( *p == '\n' ) p++;
	}
	if ( n > 0 ) goto loop2;

	// do not merge so much
	//if ( g_conf.m_indexdbMinFilesToMerge < 100 )
	//	g_conf.m_indexdbMinFilesToMerge = 100;
	if ( g_conf.m_clusterdbMinFilesToMerge < 100 )
		g_conf.m_clusterdbMinFilesToMerge = 100;
	//g_conf.m_clusterdbMaxDiskPageCacheMem = 0;
	//g_conf.m_indexdbMaxCacheMem = 0;
	//g_conf.m_clusterdbMaxCacheMem = 0;

	g_indexdb.init ();
	g_clusterdb.init();
	//g_collectiondb.init(true);
	g_indexdb.getRdb()->addRdbBase1 ( coll );
	g_clusterdb.getRdb()->addRdbBase1 ( coll );
	// this what set to 2 on me before, triggering a huge merge
	// every dump!!! very bad, i had to gdb to each process and set
	// this value to 50 myself.
	//CollectionRec *cr = g_collectiondb.getRec ( coll );
	//if ( cr->m_indexdbMinFilesToMerge < 50 )
	//	cr->m_indexdbMinFilesToMerge = 50;

	// note it
	logf(LOG_INFO,"db: Loaded %"INT64" docids from file \"%s\".",
	     dcount,filename);

	// now scan indexdb and remove recs with docids in this hash table
	logf(LOG_INFO,"db: Scanning indexdb and removing recs.");
	//logf(LOG_INFO,"db: Tight merge indexdb to make this faster.");

	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;

	key_t startKey;
	key_t endKey;
	startKey.setMin();
	endKey.setMax();

	g_threads.disableThreads();
	Rdb *r = g_indexdb.getRdb();
	collnum_t collnum = g_collectiondb.getCollnum ( coll );
	// do not start if any indexdb recs in tree or more than 1 disk file
	RdbBase *base = r->getBase(collnum);
	if ( base->getNumFiles() > 1 ) {
		log("db: More than 1 indexdb file. Please tight merge.");
		return;
	}
	if ( g_indexdb.getRdb()->m_tree.getNumUsedNodes() ) {
		log("db: Indexdb tree not empty. Please dump.");
		return;
	}

	// set niceness really high
	if ( setpriority ( PRIO_PROCESS, getpid() , 20 ) < 0 ) 
		log("db: Call to setpriority failed: %s.",
		    mstrerror(errno));


	// get a meg at a time
	int32_t minRecSizes = 5*1024*1024;

	Msg5 msg5;
	RdbList list;

	//
	//
	// SCAN INDEXDB and remove missing docids
	//
	//

	r = g_indexdb.getRdb();
	int64_t count = 0;
	int32_t scanned = 0;
	int64_t recs = 0;
	int64_t removed = 0;
	RdbTree *tree = &r->m_tree;
	CollectionRec *cr = g_collectiondb.getRec(coll);

 loop3:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_INDEXDB   ,
			      cr->m_collnum      ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      // HACK: use false for now
			      //false         , // includeTree   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      // HACK: use 1 for now
			      //1            , // numFiles      ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// something to log
	scanned += list.getListSize();
	if ( scanned >= 100000000 ) {
		count += scanned;
		scanned = 0;
		logf(LOG_INFO,"db: Scanned %"INT64" bytes. Scanned %"INT64" records. "
		     "Removed %"INT64" records.",count,recs,removed);
	}
	// yield every 256k records
	int32_t ymask = 0x40000;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		recs++;
		if ( (recs & ymask) == 0x00 ) sched_yield();
		key_t k    = list.getCurrentKey();
		// skip deletes
		if ( (k.n0 & 0x01) == 0x00 ) continue;
		uint64_t d = g_indexdb.getDocId(k);
		// see if docid is in delete list
		int32_t n = (uint32_t)d & mask;
		while ( slots[n] && slots[n] != d )
			if ( ++n >= numSlots ) n = 0;
		// skip him if we should not delete him
		if ( slots[n] != d ) continue;
		// otherwise, remove him
		// make him a delete, turn off his last bit (the del bit)
		k.n0 &= 0xfffffffffffffffeLL;
		if ( ! r->addRecord ( collnum , (char *)&k , NULL , 0 , 0) ) {
			log("db: Could not delete record.");
			return;
		}
		removed++;
		// dump tree?
		if ( tree->getNumAvailNodes() <= 0 ) {
			// this should block
			r->dumpTree(0);
		}
	}

	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey >= *(key_t *)list.getLastKey() ) goto loop3;


	logf(LOG_INFO,"db: Scanned %"INT64" bytes. Scanned %"INT64" records. "
	     "Removed %"INT64" records.",count+scanned,recs,removed);

	// this should block
	//r->dumpTree(0);

	// save the tree man!
	logf(LOG_INFO,"db: Finished removing docids from indexdb. Saving.");
	r->close ( NULL , NULL , false , false );

	//
	//
	// SCAN CLUSTERDB and remove missing docids
	//
	//

	logf(LOG_INFO,"db: Scanning clusterdb and removing recs.");
	r = g_clusterdb.getRdb();
	count = 0;
	scanned = 0;
	recs = 0;
	removed = 0;
	tree = &r->m_tree;
	//CollectionRec *cr = g_collectiondb.getRec(coll);

 loop5:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_CLUSTERDB ,
			      cr->m_collnum    ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// something to log
	scanned += list.getListSize();
	if ( scanned >= 100000000 ) {
		count += scanned;
		scanned = 0;
		logf(LOG_INFO,"db: Scanned %"INT64" bytes. Scanned %"INT64" records. "
		     "Removed %"INT64" records.",count,recs,removed);
	}
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		recs++;
		if ( (recs & ymask) == 0x00 ) sched_yield();
		key_t k    = list.getCurrentKey();
		// skip deletes
		if ( (k.n0 & 0x01) == 0x00 ) continue;
		uint64_t d = g_clusterdb.getDocId(&k);
		// see if docid is in delete list
		int32_t n = (uint32_t)d & mask;
		while ( slots[n] && slots[n] != d )
			if ( ++n >= numSlots ) n = 0;
		// skip him if we should not delete him
		if ( slots[n] != d ) continue;
		// otherwise, remove him
		// make him a delete, turn off his last bit (the del bit)
		k.n0 &= 0xfffffffffffffffeLL;
		if ( ! r->addRecord ( collnum , (char *)&k , NULL , 0 , 0) ) {
			log("db: Could not delete record.");
			return;
		}
		removed++;
		// dump tree?
		if ( tree->getNumAvailNodes() <= 0 ) {
			// this should block
			r->dumpTree(0);
		}
	}

	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey >= *(key_t *)list.getLastKey() ) goto loop5;

	logf(LOG_INFO,"db: Scanned %"INT64" bytes. Scanned %"INT64" records. "
	     "Removed %"INT64" records.",count+scanned,recs,removed);

	// this should block
	//r->dumpTree(0);

	logf(LOG_INFO,"db: Finished removing docids from clusterdb. Saving.");
	r->close ( NULL , NULL , false , false );

	return;
}

// . diff with indexdb in sync/ dir
// . returns false if they differ, true otherwise
/*
bool syncIndexdb ( ) {
	// open indexdb in sync/ dir
	Indexdb idb;
	// temporarily set the working dir
	char newdir [ 256 ];
	sprintf ( newdir , "%s/sync", g_hostdb.m_dir );
	char olddir [ 256 ];
	strcpy ( olddir , g_hostdb.m_dir );
	strcpy ( g_hostdb.m_dir , newdir );
	// init the second indexdb with this new directory
	if ( ! idb.init() ) return false;
	//if ( ! idb.getRdb()->addRdbBase1 ( "main" ) ) return false;
	// restore working dir
	strcpy ( g_hostdb.m_dir , olddir );
	// count diffs
	int64_t count = 0;
	// always block
	g_threads.disableThreads();

	// reset some stuff
	key_t nextKey; nextKey.setMin();
	RdbList ilist1;
	RdbList ilist2;
	ilist1.reset();
	ilist2.reset();

	// now read list from sync dir, and make sure in old dir
 loop:
	key_t endKey; endKey.setMax();
	// always clear last bit of g_nextKey
	nextKey.n0 &= 0xfffffffffffffffeLL;
	// announce startKey
	log("db: next k.n1=%08"XINT32" n0=%016"XINT64"",nextKey.n1,nextKey.n0);
	// a niceness of 0 tells it to block until it gets results!!
	Msg5 msg5;
	if ( ! msg5.getList ( RDB_INDEXDB    ,
			      coll1          ,
			      &ilist1        ,
			      nextKey        ,
			      endKey         , // should be maxed!
			      1024*1024      , // min rec sizes
			      true           , // include tree?
			      false          , // includeCache
			      false          , // addToCache
			      0              , // startFileNum
			      -1             , // m_numFiles   
			      NULL           , // state 
			      NULL           , // callback
			      0              , // niceness
			      true           ))// do error correction?
		return log(LOG_LOGIC,"db: getList did not block.");

	if ( ! msg5.getList ( RDB_INDEXDB    ,
			      coll2          ,
			      &ilist2        ,
			      nextKey        ,
			      endKey         , // should be maxed!
			      1024*1024      , // min rec sizes
			      true           , // include tree?
			      false          , // includeCache
			      false          , // addToCache
			      0              , // startFileNum
			      -1             , // m_numFiles   
			      NULL           , // state 
			      NULL           , // callback
			      0              , // niceness
			      true           ))// do error correction?
		return log(LOG_LOGIC,"db: getList did not block.");

	// get last keys of both
	key_t last1 ;
	key_t last2 ;
	if ( ! ilist1.isEmpty() ) last1 = ilist1.getLastKey();
	else                      last1.setMax();
	if ( ! ilist2.isEmpty() ) last2 = ilist2.getLastKey();
	else                      last2.setMax();

	// get the min
	key_t min = last1; if ( min > last2 ) min = last2;

	// now compare the two lists
 iloop:
	key_t k1; 
	key_t k2;
	// skip if both empty
	if ( ilist1.isExhausted() && ilist2.isExhausted() ) goto done;
	// if one list is exhausted before the other, dump his keys
	if ( ilist1.isExhausted() ) k1.setMax();
	else                        k1 = ilist1.getCurrentKey();
	if ( ilist2.isExhausted() ) k2.setMax();
	else                        k2 = ilist2.getCurrentKey();
	// if different report it
	if      ( k1 < k2 ) {
		log("db: sync dir has k.n1=%08"XINT32" n0=%016"XINT64"",k1.n1,k1.n0);
		ilist1.skipCurrentRecord();
		count++;
		goto iloop;
	}
	else if ( k2 < k1 ) {
		log("db: orig dir has k.n1=%08"XINT32" n0=%016"XINT64"",k2.n1,k2.n0);
		ilist2.skipCurrentRecord();
		count++;
		goto iloop;
	}
	if ( ! ilist1.isExhausted() ) ilist1.skipCurrentRecord();
	if ( ! ilist2.isExhausted() ) ilist2.skipCurrentRecord();
	goto iloop;

 done:
	// if both lists were completely empty, we're done
	if ( ilist1.isEmpty() && ilist2.isEmpty() ) {
		log("db: *-*-*-* found %"INT32" discrepancies",count);
		g_threads.enableThreads();
		return (count==0);
	}

	// advance nextKey to get next pair of lists
	nextKey = min;
	nextKey += (uint32_t)1;
	// start it all over again
	goto loop;
}
*/

/*
// generates clusterdb from titledb
bool makeClusterdb ( char *coll ) {
	key_t nextKey;
	key_t endKey;
	RdbList list;
	RdbList rlist;
	Msg5 msg5;
	Msg5 msg5b;
	int32_t minRecSizes = 1024*1024;
	//int32_t minRecSizes = 32*1024;
	uint32_t count = 0;
	// make sure the files are clean
	BigFile f;
	f.set ( g_hostdb.m_dir , "clusterdb-saved.dat");
	if ( f.doesExist() ) {
		log("db: %sclusterdb-saved.dat exists. "
		    "Not generating clusterdb.",
		    g_hostdb.m_dir);
		return false;
	}
	f.set ( g_hostdb.m_dir , "clusterdb0001.dat");
	if ( f.doesExist() ) {
		log("db: %sclusterdb0001.dat exists. Not generating clusterdb.",
		    g_hostdb.m_dir);
		return false;
	}
	// turn off threads
	g_threads.disableThreads();
	// log the start
	log("db: Generating clusterdb for Collection %s.", coll);
	// how many are we processing?
	log("db: makeclusterdb: processing %"INT32" urls",
	    g_titledb.getLocalNumDocs());

	// reset some stuff
	nextKey.n1 = 0;
	nextKey.n0 = 0;
	endKey.setMax();

	rlist.set ( NULL,
		    0,
		    NULL,
		    0,
		    0,
		    false,
		    true );

loop:
	list.reset();
	// always clear last bit of g_nextKey
	nextKey.n0 &= 0xfffffffffffffffeLL;
	//int64_t startTime = gettimeofdayInMilliseconds();
	// a niceness of 0 tells it to block until it gets results!!
	bool status = msg5.getList ( 
				 RDB_TITLEDB    ,
				 coll           ,
				 &list          ,
				 nextKey        ,
				 endKey         , // should be maxed!
				 minRecSizes    , // get this many bytes of rec
				 true           , // include tree?
				 false          , // includeCache
				 false          , // addToCache
				 0              , // startFileNum
				 -1             , // m_numFiles   
				 NULL           , // state 
				 NULL           , // callback
				 0              , // niceness
				 true           , // do error correction?
				 NULL           , // cache key
				 0              , // retry num
				 -1             , // maxRetries
				 true           , // compensate for merge
				 -1LL           , // sync point
				 &msg5b         );
	if ( ! status ) {
		log("db: critical error. msg5 did a non-blocking call");
		exit(-1);
	}

	// close up if no titleRec
	if ( list.isEmpty() ) {
		log ( LOG_INFO, "db: Added %"INT32" files to clusterdb.", count);
		log ( LOG_INFO,
		      "db: All done generating clusterdb. Saving files.");
		// force tree dump to disk
		g_clusterdb.getRdb()->dumpTree(0);
		// dump trees we did
		g_clusterdb.getRdb()->close ( NULL, NULL, true, false );
		g_threads.enableThreads();
		return true;
	}
	list.resetListPtr();
	rlist.reset();
listLoop:
	if ( list.isExhausted() ) {
		// . add our list to rdb
		if ( ! g_clusterdb.getRdb()->addList ( coll, &rlist ) ) {
			log ( "db: clusterdb addList had error: %s",
			      mstrerror(g_errno) );
			return false;
		}
		goto loop;
	}
	// advance g_nextKey to get next titleRec
	nextKey = list.getCurrentKey();
	nextKey += 1;
	// advance one if positive, must always start on a negative key
	if ( (nextKey.n0 & 0x01) == 0x01 ) nextKey += (uint32_t)1;
	// get raw rec from list
	char *rec     = list.getCurrentRec();
	int32_t  recSize = list.getCurrentRecSize();
	// set the titleRec we got
	TitleRec oldtr ;
	if ( ! oldtr.set ( rec , recSize , false ) ) {// own data?
		log("db: error setting titleRec! skipping." );
		list.skipCurrentRecord();
		goto listLoop;
	}
	Url       *url         = oldtr.getUrl();
	// log the url
	//if ( count % 1000 == 0 )
		//log(LOG_INFO, "%"INT32") %s %"INT32"",
		//		 count,url->getUrl(),oldtr.getContentLen());
	count++;
	// make a cluster rec
	char crec [ CLUSTER_REC_SIZE ];
	g_clusterdb.makeRecFromTitleRec ( crec , 
					 &oldtr,
					  false  );
	//g_clusterdb.makeRecFromTitleRecKey ( crec , 
	//				     rec,
	//				     false  );
	rlist.addRecord ( crec, 0, NULL );
	int32_t nLinkTexts = oldtr.getLinkInfo()->getNumInlinks();
	if ( nLinkTexts > 10 )
		log ( LOG_INFO, "db: %s (%"INT32" links)",
			        url->getUrl(), nLinkTexts );
	if ( count % 10000 == 0 )
		log(LOG_INFO, "db: %"INT32") %"XINT32" %"XINT64"", count,
			      ((key_t*)crec)->n1, ((key_t*)crec)->n0);
	// set startKey, endKey
	//key_t key1 = *(key_t *)crec;
	//key_t key2 = key1;
	// add to our g_clusterdb
	//rlist.set ( crec , 
	//	    CLUSTER_REC_SIZE , 
	//	    crec ,
	//	    CLUSTER_REC_SIZE ,
	//	    key1 , 
	//	    key2 ,
	//	    CLUSTER_REC_SIZE - 12 , 
	//	    false                 , // own data?
	//	    true                  );// use half keys?
	// . add our list to rdb
	//if ( ! g_clusterdb.getRdb()->addList ( coll, &rlist ) ) {
	//	log ( "db: clusterdb addList had error: %s",
	//	      mstrerror(g_errno) );
	//	return false;
	//}
	
	list.skipCurrentRecord();
	goto listLoop;
	//goto loop;
}
*/

// forces add the hash of the date meta tag into a range for every rec
/*
bool genDateRange ( char *coll ) {
	key_t nextKey;
	key_t endKey;
	RdbList list;
	RdbList rlist;
	Msg5 msg5;
	Msg5 msg5b;
	Msg1 msg1;
	int32_t minRecSizes = 1024*1024;
	//int32_t minRecSizes = 32*1024;
	uint32_t count = 0;
	uint64_t addSize = 0;
	// turn off threads
	g_threads.disableThreads();
	// log the start
	log("db: Generating date range index for Collection %s.", coll);
	// how many are we processing?
	log("db: genDateRange: processing %"INT32" urls",
	    g_titledb.getLocalNumDocs());

	// get site rec 16 for hashing date range ??
	SiteRec sr;
	sr.m_xml = g_tagdb.getSiteXml ( 16, coll, gbstrlen(coll) );

	// reset some stuff
	nextKey.n1 = 0;
	nextKey.n0 = 0;
	endKey.setMax();

	rlist.set ( NULL,
		    0,
		    NULL,
		    0,
		    0,
		    false,
		    true );

loop:
	list.reset();
	// always clear last bit of g_nextKey
	nextKey.n0 &= 0xfffffffffffffffeLL;
	//int64_t startTime = gettimeofdayInMilliseconds();
	// a niceness of 0 tells it to block until it gets results!!
	bool status = msg5.getList ( 
				 RDB_TITLEDB    ,
				 coll           ,
				 &list          ,
				 nextKey        ,
				 endKey         , // should be maxed!
				 minRecSizes    , // get this many bytes of rec
				 true           , // include tree?
				 false          , // includeCache
				 false          , // addToCache
				 0              , // startFileNum
				 -1             , // m_numFiles   
				 NULL           , // state 
				 NULL           , // callback
				 0              , // niceness
				 true           , // do error correction?
				 NULL           , // cache key
				 0              , // retry num
				 -1             , // maxRetries
				 true           , // compensate for merge
				 -1LL           , // sync point
				 &msg5b         );
	if ( ! status ) {
		log("db: critical error. msg5 did a non-blocking call");
		exit(-1);
	}

	// close up if no titleRec
	if ( list.isEmpty() ) {
	// FOR SMALL TEST ONLY!!
	//if ( list.isEmpty() || count > 500 ) {
		//log ( LOG_INFO, "db: THIS WAS ONLY A TEST OF 500 RECS!" );
		log ( LOG_INFO, "db: Generated date range for %"INT32" TitleRecs.",
				count);
		log ( LOG_INFO,
		      "db: All done generating date range. Saving files. "
		      "(%"UINT64")", addSize );
		// dump trees we did
		// force tree dump to disk
		g_indexdb.getRdb()->dumpTree(0);
		g_indexdb.getRdb()->close ( NULL, NULL, true, false );
		g_threads.enableThreads();
		return true;
	}
	list.resetListPtr();
	rlist.reset();
listLoop:
	if ( list.isExhausted() ) {
		goto loop;
	}
	// advance g_nextKey to get next titleRec
	nextKey = list.getCurrentKey();
	nextKey += 1;
	// advance one if positive, must always start on a negative key
	if ( (nextKey.n0 & 0x01) == 0x01 ) nextKey += (uint32_t)1;
	// get raw rec from list
	char *rec      = list.getCurrentRec();
	//int32_t  listSize = list.getListSize ();
	int32_t  recSize  = list.getCurrentRecSize();
	// set the titleRec we got
	TitleRec oldtr ;
	if ( ! oldtr.set ( rec , recSize , false ) ) { // own data?
		log("gotList: error setting titleRec! skipping." );
		goto loop;
	}
	// log the url
	Url *url = oldtr.getUrl();
	if ( count % 10000 == 0 )
		log(LOG_INFO, "%"INT32") %s %"INT32"",
		    count,url->getUrl(),oldtr.getContentLen());
	count++;
	// use XmlDoc and TermTable to hash the date range
	TermTable tt;
	XmlDoc    xmlDoc;
	xmlDoc.set(&oldtr, &sr, NULL, 0);
	xmlDoc.hashDate ( &tt, &oldtr, &sr );
	// dump the term table into an index list
	IndexList indexList;
	IndexList newDateList;
	uint64_t chksum1;
	indexList.set ( &tt,
			oldtr.getDocId(),
			NULL,
			&newDateList,
			0,
			NULL,
			&chksum1 ,
			0 ); // niceness
	addSize += indexList.getListSize();
	// . add our list to rdb
	if ( ! g_indexdb.getRdb()->addList ( coll, &indexList ) ) {
		log ( "db: indexdb addList had error: %s",
		      mstrerror(g_errno) );
		return false;
	}
	// go to the next titlerec
	list.skipCurrentRecord();
	goto listLoop;
}
*/

static int keycmp(const void *, const void *);
int keycmp ( const void *p1 , const void *p2 ) {
	// returns 0 if equal, -1 if p1 < p2, +1 if p1 > p2
	if ( *(key_t *)p1 < *(key_t *)p2 ) return -1;
	if ( *(key_t *)p1 > *(key_t *)p2 ) return  1;
	return 0;
}

/*
bool matchertest ( int argc, char* argv[] ) {
	const int iterCompile = 10000;
	int numTerms = -1;
	// find -- separator
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			numTerms = i;
			break;
		}
	}
	if (numTerms == -1)
		return false;
	MatchTerm terms[numTerms];
	for (int i = 0; i < numTerms; i++) {
		terms[i].m_term = (uint8_t*) argv[i];
		terms[i].m_termSize = gbstrlen(argv[i]);
	}

	// --------------------------------------------------------------------
	// do times compiles of various types
	struct timeval tv;

	// --------------------------------------------------------------------
	gettimeofday(&tv, NULL);
	uint64_t tBMMStart = tv.tv_sec * 1000000 + tv.tv_usec;
	for (int i = 0; i < iterCompile; i++) {
		BitMatrixMatcher matcher;
		matcher.Compile(terms, numTerms, false);
		if (!matcher.Ready()) {
			fprintf(stderr, "BitMatrixMatcher compile\n");
			return false;
		}
	}
	gettimeofday(&tv, NULL);
	uint64_t tBMMElapsed = (tv.tv_sec * 1000000 + tv.tv_usec) - tBMMStart;
	fprintf(stderr, "STAT %24s %6llduS Compile/Free\n",
		"BitMatrixMatcher", tBMMElapsed / iterCompile);

	// --------------------------------------------------------------------
	gettimeofday(&tv, NULL);
	uint64_t tSATMStart = tv.tv_sec * 1000000 + tv.tv_usec;
	for (int i = 0; i < iterCompile; i++) {
		SmallAsciiTrieMatcher matcher;
		matcher.Compile(terms, numTerms, false);
		if (!matcher.Ready()) {
			fprintf(stderr, "SmallAsciiTrieMatcher compile\n");
			return false;
		}
	}
	gettimeofday(&tv, NULL);
	uint64_t tSATMElapsed = (tv.tv_sec * 1000000 + tv.tv_usec) - tSATMStart;
	fprintf(stderr, "STAT %24s %6llduS Compile/Free\n",
		"SmallAsciiTrieMatcher", tSATMElapsed / iterCompile);

	// --------------------------------------------------------------------
	gettimeofday(&tv, NULL);
	uint64_t tMBTMStart = tv.tv_sec * 1000000 + tv.tv_usec;
	for (int i = 0; i < iterCompile; i++) {
		MediumBinaryTrieMatcher matcher;
		matcher.Compile(terms, numTerms, false);
		if (!matcher.Ready()) {
			fprintf(stderr, "MediumBinaryTrieMatcher compile\n");
			return false;
		}
	}
	gettimeofday(&tv, NULL);
	uint64_t tMBTMElapsed = (tv.tv_sec * 1000000 + tv.tv_usec) - tMBTMStart;
	fprintf(stderr, "STAT %24s %6llduS Compile/Free\n",
		"MediumBinaryTrieMatcher", tMBTMElapsed / iterCompile);

	// --------------------------------------------------------------------
	gettimeofday(&tv, NULL);
	uint64_t tMMStart = tv.tv_sec * 1000000 + tv.tv_usec;
	for (int i = 0; i < iterCompile; i++) {
		MatrixMatcher matcher;
		matcher.Compile(terms, numTerms, false);
		if (!matcher.Ready()) {
			fprintf(stderr, "MatrixMatcher compile\n");
			return false;
		}
	}
	gettimeofday(&tv, NULL);
	uint64_t tMMElapsed = (tv.tv_sec * 1000000 + tv.tv_usec) - tMMStart;
	fprintf(stderr, "STAT %24s %6llduS Compile/Free\n",
		"MatrixMatcher", tMMElapsed / iterCompile);

	// --------------------------------------------------------------------
	// get contents of each file into memory
	argv += (numTerms + 1);
	argc -= (numTerms + 1);
	int numFiles = argc;
	uint8_t* content[numFiles];
	uint32_t len[numFiles];
	for (int i = 0; i < numFiles; i++) {
		FILE *pf = fopen(argv[i], "rb");
		if (pf == NULL) {
			fprintf(stderr, "unable to open '%s'\n",
				argv[i]);
			return false;
		}
		struct stat sb;
		if (fstat(fileno(pf), &sb) != 0) {
			fprintf(stderr, "unable to stat '%s'\n", argv[i]);
			return false;
		}
		len[i] = sb.st_size;
		content[i] = (uint8_t*) mmalloc(len[i], "file");
		if (content == NULL) {
			fprintf(stderr, "unable to malloc '%s'\n", argv[i]);
			return false;
		}
		if (fread(content[i], len[i], 1, pf) != 1) {
			fprintf(stderr, "unable to fread '%s'\n", argv[i]);
			return false;
		}
		fclose(pf);
	}

	// --------------------------------------------------------------------
	// compile a matcher of each type
	BitMatrixMatcher matcherBMM;
	matcherBMM.Compile(terms, numTerms, false);
	//matcherBMM.Dump();
	SmallAsciiTrieMatcher matcherSATM;
	matcherSATM.Compile(terms, numTerms, false);
	//matcherSATM.Dump();
	MediumBinaryTrieMatcher matcherMBTM;
	matcherMBTM.Compile(terms, numTerms, false);
	//matcherMBTM.Dump();
	MatrixMatcher matcherMM;
	matcherMM.Compile(terms, numTerms, false);
	//matcherMM.Dump();

	const int numMatchers = 4;
	Matcher* matchers[numMatchers]  =	{	&matcherBMM,
							&matcherSATM,
							&matcherMBTM,
							&matcherMM,
						};
	char* matcherNames[numMatchers] = 	{
							"BitMatrixMatcher", 
							"SmallAsciiTrieMatcher",
							"MediumBinaryTrieMatcher",
							"MatrixMatcher"
						};

	
	// --------------------------------------------------------------------
	// perform matching on each file using each type of matcher
	const int iterExec = 1000;
	for (int fileix = 0; fileix < numFiles; fileix++) {
		for (int matcherix = 0; matcherix < numMatchers; matcherix++) {
			int hits = 0;
			gettimeofday(&tv, NULL);
			uint64_t tStart = tv.tv_sec * 1000000 + tv.tv_usec;
			for (int iter = 0; iter < iterExec; iter++) {
				hits = 0;
				const uint8_t* icursor = content[fileix];
				const uint8_t* iend = icursor + len[fileix];
				Matcher* matcher = matchers[matcherix];
				uint16_t termNum;
				while (icursor < iend) {
					icursor = matcher->Exec(icursor,
								iend - icursor,
								&termNum);
					hits++;
					if (icursor == NULL)
						break;
					//fprintf(stderr, "hit: %s\n",
					//	terms[termNum].m_term);
					icursor += terms[termNum].m_termSize;
				}
			}
			gettimeofday(&tv, NULL);
			uint64_t tElapsed = (tv.tv_sec * 1000000 + tv.tv_usec) -
				tStart;
			fprintf(stderr,"STAT %24s %6llduS %4dKB %4d hits %s\n",
				matcherNames[matcherix],
				tElapsed / iterExec, len[fileix] / 1024,
				hits - 1, argv[fileix]);
		}
	}

	return true;
}

bool trietest ( ) {
	//TrieMatcher<uint8_t,8> matcher;
	MatrixMatcher matcher;
	MatchTerm terms[3];
	terms[0].m_term = (uint8_t*) "jackie";
	terms[0].m_termSize = 6;
	terms[1].m_term = (uint8_t*) "jack";
	terms[1].m_termSize = 4;
	terms[2].m_term = (uint8_t*) "sandi";
	terms[2].m_termSize = 5;
	matcher.Compile(terms, 3, false);
	matcher.Dump();
	uint16_t numTerm;
	const uint8_t* pos;
	#define STRING (uint8_t*) "this is jAck's test for Sandi's enjoyment"
	for (int i = 0; i < 10; i++) {
		pos = matcher.Exec(STRING, gbstrlen((char*) STRING), &numTerm);
		if (pos != NULL) {
			fprintf(stderr, "term[%d] '%s' -> %s\n",
				numTerm, terms[numTerm].m_term, pos);
			pos += gbstrlen((char*) terms[numTerm].m_term);
			pos = matcher.Exec(pos, gbstrlen((char*) pos), &numTerm);
			if (pos != NULL) {
				fprintf(stderr, "term[%d] '%s' -> %s\n",
					numTerm, terms[numTerm].m_term, pos);
				pos += gbstrlen((char*) terms[numTerm].m_term);
				pos = matcher.Exec(pos, gbstrlen((char*) pos),
						&numTerm);
				if (pos != NULL)
					exit(1);
			}
		}
	}
	return false;
}
*/

// time speed of inserts into RdbTree for indexdb
bool bucketstest ( char* dbname ) {
	g_conf.m_maxMem = 2000000000LL; // 2G
	//g_mem.m_maxMem  = 2000000000LL; // 2G


	if ( dbname ) {
		char keySize = 16;
		if(strcmp(dbname, "indexdb") == 0) keySize = 12;
		RdbBuckets rdbb;
		rdbb.set (0, 
			  0x7fffffff, // LONG_MAX , 
			  false ,//own data
			  "buckets-test",
			  RDB_INDEXDB,
			  false , //data in ptrs
			  "TestBuckets" , 
			  keySize , 
			  false );
		rdbb.loadBuckets ( dbname );
		if(!rdbb.selfTest(true/*testall*/, false/*core*/)) 
			if(!rdbb.repair())
				log("db: unrepairable buckets.");
		return 0;
	}

	char oppKey[MAX_KEY_BYTES];
	RdbBuckets rdbb;
	char keySize = 12;
	rdbb.set (0, 
		  0x7fffffff,//LONG_MAX , 
		  false ,//own data
		  "buckets-test",
		  RDB_INDEXDB,
		  false , //data in ptrs
		  "TestBuckets" , keySize , false );


	int32_t numKeys = 1000000;
	log("db: speedtest: generating %"INT32" random keys.",numKeys);
	// seed randomizer
	srand ( (int32_t)gettimeofdayInMilliseconds() );
	// make list of one million random keys
	char *k = (char*)mmalloc ( keySize * numKeys , "main" );
	if ( ! k ) return log("speedtest: malloc failed");
	int32_t *r = (int32_t *)k;
	int32_t ksInLongs = keySize / 4;
	for ( int32_t i = 0 ; i < numKeys * ksInLongs ; i++ ) {
		r[i] = rand();// % 2000;
	}

	for ( int32_t i = 0 ; i < 1000 ; i++ ) {
		int32_t j = (rand() % numKeys) * keySize;
		int32_t m = (rand() % numKeys) * keySize;
		gbmemcpy((char*)&k[j], (char*)&k[m], keySize);
		KEYXOR((char*)&k[j],0x01);
	}

	// init the tree
	RdbTree rt;
	if ( ! rt.set ( 0              , // fixedDataSize  , 
			numKeys + 1000 , // maxTreeNodes   ,
			false          , // isTreeBalanced , 
			numKeys * 32   , // maxTreeMem     ,
			false          ,
			"tree-test"    ,
			false          ,
			"TestTree"     ,
			keySize) ) // own data?
		return log("speedTest: tree init failed.");
	// add to regular tree
	int64_t t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < numKeys * keySize; i += keySize ) {
		char* key = k+i;
		KEYSET(oppKey,key,keySize);
		KEYXOR(oppKey,0x01);
		int32_t n;
		n = rt.getNode ( 0, oppKey );
		if ( n >= 0 ) {
			rt.deleteNode3 ( n , true );
		}
		if ( rt.addNode ( 0, key , NULL , 0 ) < 0 )
			return log("speedTest: rdb tree addNode "
				   "failed");
	}
	// print time it took
	int64_t e = gettimeofdayInMilliseconds_force();
	log("db: added %"INT32" keys to rdb tree in %"INT64" ms, "
	    "now have %"INT32" keys",numKeys,e - t, rt.getNumUsedNodes());

	for ( int32_t i = 0 ; i < numKeys * keySize; i+=keySize ) {
		char* key = k+i;
		//if ( k[i].n1 == 1234567 )
		//	fprintf(stderr,"i=%"INT32"\n",i);
		if ( rdbb.addNode ( 0,key , NULL , 0 ) < 0 )
			return log("speedTest: rdb buckets addNode "
				   "failed");
	}

	rdbb.testAndRepair();
	t = gettimeofdayInMilliseconds_force();
	log("db: added %"INT32" keys to rdb buckets in %"INT64" ms, "
	    "now have %"INT32" keys, mem used: %"INT32"",
	    numKeys,t - e, rdbb.getNumKeys(),rdbb.getMemOccupied());
	rdbb.selfTest(true, true);


	log("db: saving and loading buckets.");
	    
	e = gettimeofdayInMilliseconds_force();

 	rdbb.fastSave ( ".",
 			false,
 			NULL, NULL);

	t = gettimeofdayInMilliseconds_force();
	log("db: saved rdbbuckets in %"INT64" ms",t - e);

	//rdbb.setNeedsSave(false);
	rdbb.clear();
	e = gettimeofdayInMilliseconds_force();

 	rdbb.loadBuckets ( "TestBuckets" );

	t = gettimeofdayInMilliseconds_force();
	log("db: loaded rdbbuckets in %"INT64" ms", t - e);
	
	rdbb.selfTest(true, true);

	//now test loading a tree, the keys will be sorted, so this
	// is the worst case performance.
 	RdbBuckets rdbb2;
 	rdbb2.set (0, 
 		   10000000 , 
 		   false ,//own data
		   "buckets-test",
		   RDB_INDEXDB,
 		   false , //data in ptrs
 		   "TestBuckets" , keySize , false );

  	rdbb2.addTree (&rt);
  	rdbb2.selfTest(true, true);
	rdbb2.setNeedsSave(false);


	//now test finding of individual keys
	int32_t tests = numKeys * 2;
	log("db: Testing retrival of %"INT32" individual keys",tests );
	int64_t ttook = 0;
	int64_t btook = 0;
	int32_t tgot = 0;
	int32_t bgot = 0;
	int32_t found = 0;
	for ( int32_t i = 0 ; i < 0; i++ ) {
		int32_t j = (rand() % numKeys) * keySize;
		e = gettimeofdayInMilliseconds_force();
		int32_t nodeNum = rt.getNode ( 0 , (char*)&k[j]);
		t = gettimeofdayInMilliseconds_force();
		ttook += t - e;
		
		e = gettimeofdayInMilliseconds_force();
		char* foundKey = rdbb.getKeyVal ( 0 , (char*)&k[j], NULL, NULL);
		t = gettimeofdayInMilliseconds_force();
		btook += t - e;


		if(nodeNum == -1) {
			if(foundKey == NULL) {
				continue;
			}
			log("speedTest: node not found in tree, but found in buckets! "
			    "looked up %016"XINT64"%08"XINT32", got %016"XINT64"%08"XINT32"",
			    *(int64_t*)((char*)&k[j]+(sizeof(int32_t))),
			    *(int32_t*)(char*)&k[j],
			    *(int64_t*)(foundKey+(sizeof(int32_t))),
			    *(int32_t*)foundKey);
			rdbb.printBuckets();
			char* xx = NULL; *xx = 0;
		}
		if(foundKey == NULL) {
			if(nodeNum == -1) {
				continue;
			}
			log("speedTest: node not found in buckets, but found in tree! "
			    "%016"XINT64"%08"XINT32"",
			    *(int64_t*)((char*)&k[j]+(sizeof(int32_t))),
			    *(int32_t*)(char*)&k[j]);

			rdbb.printBuckets();
			char* xx = NULL; *xx = 0;
		}
		found++;
	}
	log("db: found %"INT32" keys from rdbtree in %"INT64" ms",found, ttook);
	log("db: found %"INT32" keys from rdbbuckets in %"INT64" ms",found, btook);


	// sort the list of keys
	t = gettimeofdayInMilliseconds_force();
	gbsort ( k , numKeys , sizeof(key_t) , keycmp );
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("db: sorted %"INT32" in %"INT64" ms",numKeys,e - t);


	tests = 100;
	log("db: Testing retrival of a list of keys, %"INT32" random ranges", tests);
	RdbList treelist;
	RdbList bucketlist;
	RdbList list;
	int32_t numPosRecs;
	int32_t numNegRecs;
	char *tmpkey1;
	char *tmpkey2;
	char key1 [ MAX_KEY_BYTES ];
	char key2 [ MAX_KEY_BYTES ];


	int32_t minRecSizes = 10000000;
	//int32_t minRecSizes = -1;
	for ( int32_t i = 0 ; i < tests; i++ ) {
// 		int32_t startKey = rand() % numKeys;
// 		int32_t endKey = (rand() % (numKeys - startKey)) + startKey;
		for ( int32_t x = 0 ; x < MAX_KEY_BYTES; x++ ) {
			key1[x] = rand();
			key2[x] = rand();
		}
 		char* skey;
 		char* ekey;

		if ( KEYCMP(key1,key2,keySize) < 0 ) {
			skey = key1;
			ekey = key2;
		} else {
			skey = key2;
			ekey = key1;
		}
		
		e = gettimeofdayInMilliseconds_force();
		rt.getList ( (collnum_t)0 , 
			     skey, 
			     ekey, 
			     minRecSizes, //min rec sizes
			     &treelist,
			     &numPosRecs,
			     &numNegRecs,
			     true ); //use half keys
		t = gettimeofdayInMilliseconds_force();
		ttook += t - e;
		tgot += treelist.getNumRecs();

		tmpkey1 = treelist.getStartKey();
		tmpkey2 = treelist.getEndKey();

		/*
		log(LOG_WARN, "db rdbtree found %"INT32" recs (%"INT32" pos, %"INT32" neg) "
		"between "
		"%016"XINT64"%08"XINT32" and "
		"%016"XINT64"%08"XINT32".  "
		    "took %"INT64" ms",
		    treelist.getNumRecs(),
		    numPosRecs, 
		    numNegRecs,
		    *(int64_t*)(tmpkey1+(sizeof(int32_t))), *(int32_t*)tmpkey1,
		    *(int64_t*)(tmpkey2+(sizeof(int32_t))), *(int32_t*)tmpkey2,
		    t - e  );
		*/

		e = gettimeofdayInMilliseconds_force();
		rdbb.getList ( (collnum_t)0 , 
			       skey, 
			       ekey, 
			       minRecSizes, //min rec sizes
			       &bucketlist,
			       &numPosRecs,
			       &numNegRecs,
			       true ); //use half keys
		t = gettimeofdayInMilliseconds_force();
		btook += t - e;
		bgot += bucketlist.getNumRecs();


		tmpkey1 = bucketlist.getStartKey();
		tmpkey2 = bucketlist.getEndKey();

		/*
		log(LOG_WARN, "db buckets found %"INT32" recs (%"INT32" pos, %"INT32" neg) "
		"between "
		    "%016"XINT64"%08"XINT32" and "
		    "%016"XINT64"%08"XINT32".  "
		    "took %"INT64" ms",
		    bucketlist.getNumRecs(),
		    numPosRecs, 
		    numNegRecs,
		    *(int64_t*)(tmpkey1+(sizeof(int32_t))), *(int32_t*)tmpkey1,
		    *(int64_t*)(tmpkey2+(sizeof(int32_t))), *(int32_t*)tmpkey2,
		    t - e  );
		*/
		//check for consistency
		char tkey [ MAX_KEY_BYTES ];
		char bkey [ MAX_KEY_BYTES ];
		while(1) {
			if(treelist.isExhausted() ) {
				if(bucketlist.isExhausted() ) break;
				bucketlist.getCurrentKey(bkey);
				log(LOG_WARN, "db tree and buckets "
				    "inconsistency"
				    " remaining key in buckets is "
				    "%016"XINT64"%08"XINT32".  ",
				    *(int64_t*)(bkey+(sizeof(int32_t))),
				    *(int32_t*)bkey);

				char* xx = NULL; *xx = 0;
			} 
			else if (bucketlist.isExhausted() ) {
				treelist.getCurrentKey(tkey);
				log(LOG_WARN, "db tree and buckets "
				    "inconsistency"
				    " remaining key in tree is "
				    "%016"XINT64"%08"XINT32".  ",
				    *(int64_t*)(tkey+(sizeof(int32_t))),
				    *(int32_t*)tkey);
				char* xx = NULL; *xx = 0;
			}
			treelist.getCurrentKey(tkey);
			bucketlist.getCurrentKey(bkey);

			if ( KEYCMP(tkey,bkey,keySize) != 0 ) {
				log(LOG_WARN, "db tree and buckets "
				    "inconsistency "
				    "%016"XINT64"%08"XINT32" and "
				    "%016"XINT64"%08"XINT32".  ",
				    *(int64_t*)(tkey+(sizeof(int32_t))),
				    *(int32_t*)tkey,
				    *(int64_t*)(bkey+(sizeof(int32_t))),
				    *(int32_t*)bkey);
				char* xx = NULL; *xx = 0;
			}

			treelist.skipCurrentRecord();
			bucketlist.skipCurrentRecord();
		}
	}
	log("db: List retrieval successful. ");
	log("db: rdbtree took %"INT64" ms for %"INT32" recs ", ttook, tgot);
	log("db: rdbbuckets took %"INT64" ms for %"INT32" recs",   btook, bgot);
	int64_t tAddTook = 0;
	int64_t bAddTook = 0;
	int64_t tgetListTook = 0;
	int64_t bgetListTook = 0;
	int64_t tdelListTook = 0;
	int64_t bdelListTook = 0;

	ttook = 0;
	btook = 0;
	tgot = 0;
	bgot = 0;

	minRecSizes = 200000;

	
	KEYSET(key1,KEYMIN(), keySize);
	KEYSET(key2,KEYMAX(), keySize);
	bool status = true;
	log("db: simulating dump, deleting entire list of keys");
	while(rdbb.getNumKeys() > 0 && status) {
		status = rdbb.getList ( (collnum_t)0,
					key1       , 
					KEYMAX()   ,
					minRecSizes, 
					&list      , 
					&numPosRecs ,
					&numNegRecs ,
					false );
		if(!status) {char* xx = NULL; *xx = 0;}
		if ( status && list.isEmpty() ) break;
		int32_t numBefore = rdbb.getNumKeys();
		rdbb.deleteList((collnum_t)0, &list);
		//		if (KEYCMP(key2,key1,keySize) < 0) break;


		log("db: buckets now has %"INT32" keys.  "
		    "difference of %"INT32", list size was %"INT32".  "
		    "%016"XINT64"%08"XINT32".  ",
		    rdbb.getNumKeys(), numBefore - rdbb.getNumKeys(),
		    list.getNumRecs(),
		    *(int64_t*)(key1+(sizeof(int32_t))),
		    *(int32_t*)key1);;

		if(numBefore - rdbb.getNumKeys() != list.getNumRecs()) 
			{char* xx = NULL; *xx = 0;}
		KEYSET(key2,key1,keySize);
		KEYSET(key1,list.getLastKey(),keySize);
		KEYADD(key1,keySize);
	}
	if(rdbb.getNumKeys() > 0) {char* xx = NULL; *xx = 0;}


	rdbb.setNeedsSave(false);
	rdbb.clear();

	log("db: Testing retrival of a list of keys, %"INT32" random ranges "
	    "interspersed with adds and deletes", numKeys);
	rt.clear();
	rt.m_needsSave = false;

	for ( int32_t i = 0 ; i < numKeys ; i++ ) {

		e = gettimeofdayInMilliseconds_force();

		char* key = &k[i*keySize];
		KEYSET(oppKey,key,keySize);
		KEYXOR(oppKey,0x01);
		int32_t n;
		collnum_t collnum = rand() % 10;

		n = rt.getNode ( collnum , oppKey );
		if ( n >= 0 ) rt.deleteNode3 ( n , true );

		if ( rt.addNode (collnum, key, NULL , 0 ) < 0 )
			return log("speedTest: rdb tree addNode "
				   "failed");
		t = gettimeofdayInMilliseconds_force();
		tAddTook += t - e;

		e = gettimeofdayInMilliseconds_force();
		if ( rdbb.addNode(collnum, key, NULL, 0 ) < 0 )
			return log("speedTest: rdb buckets addNode "
				   "failed");
		t = gettimeofdayInMilliseconds_force();
		bAddTook += t - e;

		if(i % 100 != 0) continue;

 		char* skey;
 		char* ekey;

 		if(rand() % 2) { //check keys that exist
 			int32_t beg = (rand() % numKeys) * keySize;
 			int32_t end = (rand() % numKeys) * keySize;
 			skey = (char*)&k[beg];
 			ekey = (char*)&k[end];
 			if(KEYCMP(skey,ekey,keySize) > 0) {
 				skey = (char*)&k[end];
 				ekey = (char*)&k[beg];
 			}
 		}
 		else {//otherwise check keys that don't exist
			for ( int32_t x = 0 ; x < MAX_KEY_BYTES; x++ ) {
				key1[x] = rand();
				key2[x] = rand();
			}
			if ( KEYCMP(key1,key2,keySize) < 0 ) {
				skey = key1;
				ekey = key2;
			} else {
				skey = key2;
				ekey = key1;
			}
		}

		
		e = gettimeofdayInMilliseconds_force();
		rt.getList ( collnum, 
			     skey, 
			     ekey, 
			     minRecSizes, //min rec sizes
			     &treelist,
			     &numPosRecs,
			     &numNegRecs,
			     true ); //use half keys
		t = gettimeofdayInMilliseconds_force();
		tgetListTook += t - e;
		tgot += treelist.getNumRecs();

		if(!treelist.checkList_r(false, false, RDB_INDEXDB))
			log("tree's list was bad");


		tmpkey1 = treelist.getStartKey();
		tmpkey2 = treelist.getEndKey();


		if(treelist.getNumRecs() > 0) {
			log(LOG_WARN, "db inserted %"INT32" keys", i+1);
			log(LOG_WARN, "db rdbtree found %"INT32" recs (%"INT32" pos, "
			    "%"INT32" neg) between "
			    "%016"XINT64"%08"XINT32" and "
			    "%016"XINT64"%08"XINT32".  "
			    "took %"INT64" ms, %"INT64" ms so far",
			    treelist.getNumRecs(),
			    numPosRecs, 
			    numNegRecs,
			    *(int64_t*)(tmpkey1+(sizeof(int32_t))), *(int32_t*)tmpkey1,
			    *(int64_t*)(tmpkey2+(sizeof(int32_t))), *(int32_t*)tmpkey2,
			    t - e ,tgetListTook );
		}

		e = gettimeofdayInMilliseconds_force();
		rdbb.getList ( collnum, 
			       skey, 
			       ekey, 
			       minRecSizes, //min rec sizes
			       &bucketlist,
			       &numPosRecs,
			       &numNegRecs,
			       true ); //use half keys
		t = gettimeofdayInMilliseconds_force();
		bgetListTook += t - e;
		bgot += bucketlist.getNumRecs();

		if(!bucketlist.checkList_r(false, false, RDB_INDEXDB)) 
			log("bucket's list was bad");


		tmpkey1 = bucketlist.getStartKey();
		tmpkey2 = bucketlist.getEndKey();


		if(treelist.getNumRecs() > 0) {
			log(LOG_WARN, "db buckets found %"INT32" recs (%"INT32" pos, "
			    "%"INT32" neg) between "
			    "%016"XINT64"%08"XINT32" and "
			    "%016"XINT64"%08"XINT32".  "
			    "took %"INT64" ms, %"INT64" ms so far.",
			    bucketlist.getNumRecs(),
			    numPosRecs, 
			    numNegRecs,
			    *(int64_t*)(tmpkey1+(sizeof(int32_t))), *(int32_t*)tmpkey1,
			    *(int64_t*)(tmpkey2+(sizeof(int32_t))), *(int32_t*)tmpkey2,
			    t - e , bgetListTook);
		}

		//check for consistency
		treelist.resetListPtr();
		bucketlist.resetListPtr();
		char tkey [ MAX_KEY_BYTES ];
		char bkey [ MAX_KEY_BYTES ];
		while(1) {
			if(treelist.isExhausted() ) {
				if(bucketlist.isExhausted() ) break;
				bucketlist.getCurrentKey(bkey);
				log(LOG_WARN, "db tree and buckets "
				    "inconsistency"
				    " remaining key in buckets is "
				    "%016"XINT64"%08"XINT32".  ",
				    *(int64_t*)(bkey+(sizeof(int32_t))),
				    *(int32_t*)bkey);

				char* xx = NULL; *xx = 0;
			} 
			else if (bucketlist.isExhausted() ) {
				treelist.getCurrentKey(tkey);
				log(LOG_WARN, "db tree and buckets "
				    "inconsistency"
				    " remaining key in tree is "
				    "%016"XINT64"%08"XINT32".  ",
				    *(int64_t*)(tkey+(sizeof(int32_t))),
				    *(int32_t*)tkey);
				char* xx = NULL; *xx = 0;
			}
			treelist.getCurrentKey(tkey);
			bucketlist.getCurrentKey(bkey);

			if ( KEYCMP(tkey,bkey,keySize) != 0 ) {
				log(LOG_WARN, "db tree and buckets "
				    "inconsistency "
				    "%016"XINT64"%08"XINT32" and "
				    "%016"XINT64"%08"XINT32".  ",
				    *(int64_t*)(tkey+(sizeof(int32_t))),
				    *(int32_t*)tkey,
				    *(int64_t*)(bkey+(sizeof(int32_t))),
				    *(int32_t*)bkey);
				char* xx = NULL; *xx = 0;
			}

			treelist.skipCurrentRecord();
			bucketlist.skipCurrentRecord();
		}
		//continue;
		if(rand() % 100 != 0) continue;


		log("db: removing %"INT32" nodes from tree.  "
		"tree currently has %"INT32" keys",
		treelist.getNumRecs(), rt.getNumUsedNodes  ( ));
		e = gettimeofdayInMilliseconds_force();

		rt.deleteList(collnum, &treelist, true);
		t = gettimeofdayInMilliseconds_force();
		tdelListTook += t - e;

		log("db: Now tree has %"INT32" keys", rt.getNumUsedNodes());

		log("db: removing %"INT32" nodes from buckets. "
		"buckets currently has %"INT32" keys", 
		bucketlist.getNumRecs(), rdbb.getNumKeys(0));
		e = gettimeofdayInMilliseconds_force();
		rdbb.deleteList(collnum, &bucketlist);
		t = gettimeofdayInMilliseconds_force();
		bdelListTook += t - e;

		log("db: Now buckets has %"INT32" keys",  rdbb.getNumKeys(0));
	}


	log("db: List retrieval successful. ");
	log("db: rdbtree Add %"INT64" ms, GetList %"INT64" ms, Delete %"INT64" "
	    "for %"INT32" recs ",
	    tAddTook, tgetListTook, tdelListTook, tgot);

	log("db: rdbBuckets Add %"INT64" ms, GetList %"INT64" ms, Delete %"INT64" "
	    "for %"INT32" recs ",
	    bAddTook, bgetListTook, bdelListTook, bgot);



	#if 0
	// get the list
	key_t kk;
	kk.n0 = 0LL;
	kk.n1 = 0;
	//kk.n1 = 1234567;
	//int32_t n = rt.getNextNode ( (collnum_t)0, (char *)&kk );
	int32_t n = rt.getFirstNode();
	// loop it
	t = gettimeofdayInMilliseconds_force();
	int32_t count = 0;
	while ( n >= 0 ) {
		n = rt.getNextNode ( n );
		count++;
	}
	e = gettimeofdayInMilliseconds_force();
	log("db: getList for %"INT32" nodes in %"INT64" ms",count,e - t);
#endif
	rt.m_needsSave = false;
	rdbb.setNeedsSave(false);
	return true;
}

// time speed of inserts into RdbTree for indexdb
bool treetest ( ) {
	int32_t numKeys = 500000;
	log("db: speedtest: generating %"INT32" random keys.",numKeys);
	// seed randomizer
	srand ( (int32_t)gettimeofdayInMilliseconds_force() );
	// make list of one million random keys
	key_t *k = (key_t *)mmalloc ( sizeof(key_t) * numKeys , "main" );
	if ( ! k ) return log("speedtest: malloc failed");
	int32_t *r = (int32_t *)k;
	int32_t size = 0;
	int32_t first = 0;
	for ( int32_t i = 0 ; i < numKeys * 3 ; i++ ) {
		if ( (i % 3) == 2 && first++ < 50000 ) {
			r[i] = 1234567;
			size++;
		}
		else
			r[i] = rand();
	}
	// init the tree
	RdbTree rt;
	if ( ! rt.set ( 0              , // fixedDataSize  , 
			numKeys + 1000 , // maxTreeNodes   ,
			false          , // isTreeBalanced , 
			numKeys * 28   , // maxTreeMem     ,
			false          , // own data?
			"tree-test"    ) )
		return log("speedTest: tree init failed.");
	// add to regular tree
	int64_t t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < numKeys ; i++ ) {
		//if ( k[i].n1 == 1234567 )
		//	fprintf(stderr,"i=%"INT32"\n",i);
		if ( rt.addNode ( (collnum_t)0 , k[i] , NULL , 0 ) < 0 )
			return log("speedTest: rdb tree addNode "
				   "failed");
	}
	// print time it took
	int64_t e = gettimeofdayInMilliseconds_force();
	log("db: added %"INT32" keys to rdb tree in %"INT64" ms",numKeys,e - t);

	// sort the list of keys
	t = gettimeofdayInMilliseconds_force();
	gbsort ( k , numKeys , sizeof(key_t) , keycmp );
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("db: sorted %"INT32" in %"INT64" ms",numKeys,e - t);

	// get the list
	key_t kk;
	kk.n0 = 0LL;
	kk.n1 = 0;
	kk.n1 = 1234567;
	int32_t n = rt.getNextNode ( (collnum_t)0, (char *)&kk );
	// loop it
	t = gettimeofdayInMilliseconds_force();
	int32_t count = 0;
	while ( n >= 0 && --first >= 0 ) {
		n = rt.getNextNode ( n );
		count++;
	}
	e = gettimeofdayInMilliseconds_force();
	log("db: getList for %"INT32" nodes in %"INT64" ms",count,e - t);
	return true;
}


// time speed of inserts into RdbTree for indexdb
bool hashtest ( ) {
	// load em up
	int32_t numKeys = 1000000;
	log("db: speedtest: generating %"INT32" random keys.",numKeys);
	// seed randomizer
	srand ( (int32_t)gettimeofdayInMilliseconds_force() );
	// make list of one million random keys
	key_t *k = (key_t *)mmalloc ( sizeof(key_t) * numKeys , "main" );
	if ( ! k ) return log("speedtest: malloc failed");
	int32_t *r = (int32_t *)k;
	for ( int32_t i = 0 ; i < numKeys * 3 ; i++ ) r[i] = rand();
	// init the tree
	//HashTableT<int32_t,int32_t> ht;
	HashTable ht;
	ht.set ( (int32_t)(1.1 * numKeys) );
	// add to regular tree
	int64_t t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < numKeys ; i++ ) 
		if ( ! ht.addKey ( r[i] , 1 ) )
			return log("hashtest: add key failed.");
	// print time it took
	int64_t e = gettimeofdayInMilliseconds_force();
	// add times
	log("db: added %"INT32" keys in %"INT64" ms",numKeys,e - t);

	// do the delete test
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < numKeys ; i++ ) 
		if ( ! ht.removeKey ( r[i] ) )
			return log("hashtest: add key failed.");
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	// add times
	log("db: deleted %"INT32" keys in %"INT64" ms",numKeys,e - t);

	return true;
}


// time speed of big write, read and the seeks
bool thrutest ( char *testdir , int64_t fileSize ) {

	// always block
	g_threads.disableThreads();

	// a read/write buffer of 30M
	int32_t bufSize = 30000000;  // 30M
	//int64_t fileSize = 4000000000LL; // 4G
#undef malloc
	char *buf = (char *) malloc ( bufSize );
#define malloc coreme
	if ( ! buf ) return log("speedtestdisk: %s",strerror(errno));
	// store stuff in there
	for ( int32_t i = 0 ; i < bufSize ; i++ ) buf[i] = (char)i;

	BigFile f;
	// try a read test from speedtest*.dat*
	f.set (testdir,"speedtest");
	if ( f.doesExist() ) {
		if ( ! f.open ( O_RDONLY ) )
			return log("speedtestdisk: cannot open %s/%s",
				   testdir,"speedtest");
		// ensure big enough
		if ( f.getFileSize() < fileSize ) 
			return log("speedtestdisk: File %s/%s is too small "
				   "for requested read size.",
				   testdir,"speedtest");
		log("db: reading from speedtest0001.dat");
		f.setBlocking();
		goto doreadtest;
	}
	// try a read test from indexdb*.dat*
	f.set (testdir,"indexdb0001.dat");
	if ( f.doesExist() ) {
		if ( ! f.open ( O_RDONLY ) )
			return log("speedtestdisk: cannot open %s/%s",
				   testdir,"indexdb0001.dat");
		log("db: reading from indexdb0001.dat");
		f.setBlocking();
		goto doreadtest;
	}
	// try a write test to speedtest*.dat*
	f.set (testdir,"speedtest");
	if ( ! f.doesExist() ) {
		if ( ! f.open ( O_RDWR | O_CREAT | O_SYNC ) )
			return log("speedtestdisk: cannot open %s/%s",
				   testdir,"speedtest");
		log("db: writing to speedtest0001.dat");
		f.setBlocking();
	}

	// write  2 gigs to the file, 1M at a time
	{
	int64_t t1 = gettimeofdayInMilliseconds_force();
	int32_t numLoops = fileSize / bufSize;
	int64_t off = 0LL;
	int32_t next = 0;
	for ( int32_t i = 0 ; i < numLoops ; i++ ) {
		f.write ( buf , bufSize , off );
		sync(); // f.flush ( );
		off  += bufSize ;
		next += bufSize;
		//if ( i >= numLoops || next < 100000000 ) continue;
		if ( i + 1 < numLoops && next < 100000000 ) continue;
		next = 0;
		// print speed every X seconds
		int64_t t2 = gettimeofdayInMilliseconds_force();
		float mBps = (float)off / (float)(t2-t1) / 1000.0 ;
		fprintf(stderr,"wrote %"INT64" bytes in %"INT64" ms (%.1f MB/s)\n",
			off,t2-t1,mBps);
	}
	}
		
 doreadtest:

	{
	int64_t t1 = gettimeofdayInMilliseconds_force();
	int32_t numLoops = fileSize / bufSize;
	int64_t off = 0LL;
	int32_t next = 0;
	for ( int32_t i = 0 ; i < numLoops ; i++ ) {
		f.read ( buf , bufSize , off );
		//sync(); // f.flush ( );
		off  += bufSize ;
		next += bufSize;
		//if ( i >= numLoops || next < 100000000 ) continue;
		if ( i + 1 < numLoops && next < 100000000 ) continue;
		next = 0;
		// print speed every X seconds
		int64_t t2 = gettimeofdayInMilliseconds_force();
		float mBps = (float)off / (float)(t2-t1) / 1000.0 ;
		fprintf(stderr,"read %"INT64" bytes in %"INT64" ms (%.1f MB/s)\n",
			off,t2-t1,mBps);
	}
	}

	return true;
}

//
// SEEK TEST
//

#include <sys/time.h>  // gettimeofday()
#include <sys/time.h>
#include <sys/resource.h>
//#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//static pthread_attr_t s_attr;
//static int startUp ( void *state ) ;
static void *startUp ( void *state , ThreadEntry *t ) ;
static int32_t s_count = 0;
static int64_t s_filesize = 0;
//static int32_t s_lock = 1;
static int32_t s_launched = 0;
//static int s_fd1 ; // , s_fd2;
static BigFile s_f;
static int32_t s_numThreads = 0;
static int64_t s_maxReadSize = 1;
static int64_t s_startTime = 0;
//#define MAX_READ_SIZE (2000000)
#include <sys/types.h>
#include <sys/wait.h>

void seektest ( char *testdir, int32_t numThreads, int32_t maxReadSize , 
		char *filename ) {

	g_loop.init();
	g_threads.init();
	s_numThreads = numThreads;
	s_maxReadSize = maxReadSize;
	if ( s_maxReadSize <= 0 ) s_maxReadSize = 1;
	//if ( s_maxReadSize > MAX_READ_SIZE ) s_maxReadSize = MAX_READ_SIZE;

	log(LOG_INIT,"admin: dir=%s threads=%"INT32" maxReadSize=%"INT32" file=%s\n",
	    testdir,(int32_t)s_numThreads, (int32_t)s_maxReadSize , filename );

	// maybe its a filename in the cwd
	if ( filename ) {
		s_f.set(testdir,filename);
		if ( s_f.doesExist() ) {
			log(LOG_INIT,"admin: reading from %s.",
			    s_f.getFilename());
			goto skip;
		}
		log("admin: %s does not exists. Use ./gb thrutest ... "
		    "to create speedtest* files.",
		    s_f.getFilename());
		return;
	}
	// check other defaults
	s_f.set ( testdir , "speedtest" );
	if ( s_f.doesExist() ) {
		log(LOG_INIT,"admin: reading from speedtest*.dat.");
		goto skip;
	}
	// try a read test from indexdb*.dat*
	s_f.set (testdir,"indexdb0001.dat");
	if ( s_f.doesExist() ) {
		log(LOG_INIT,"admin: reading from indexdb0001.dat.");
		goto skip;
	}

	log("admin: Neither speedtest* or indexdb0001.dat* "
	    "exist. Use ./gb thrutest ... to create speedtest* files.");
	return;
skip:
	s_f.open ( O_RDONLY );
	s_filesize = s_f.getFileSize();
	log ( LOG_INIT, "admin: file size = %"INT64".",s_filesize);
	// always block
	//g_threads.disableThreads();
	// seed rand
	srand(time(NULL));

	//fprintf(stderr,"disabled until Threads class is used\n");
	//return;
	//}

	// open 2 file descriptors
	//s_fd1 = open ( "/tmp/glibc-2.2.2.tar" , O_RDONLY );
	//s_fd1 = open ( filename , O_RDONLY );
	//s_fd2 = open ( "/tmp/glibc-2.2.5.tar" , O_RDONLY );
	// . set up the thread attribute we use for all threads
	// . fill up with the default values first
	//if ( pthread_attr_init( &s_attr ) ) 
	//	fprintf (stderr,"Threads::init: pthread_attr_init: error\n");
	// then customize
	//if ( pthread_attr_setdetachstate(&s_attr,PTHREAD_CREATE_DETACHED) )
	//	fprintf ( stderr,"Threads::init: pthread_attr_setdeatchstate:\n");
	//if ( setpriority ( PRIO_PROCESS, getpid() , 0 ) < 0 ) {
	//	fprintf(stderr,"Threads:: setpriority: failed\n");
	//	exit(-1);
	//}
	//s_lock = 1;
	//pthread_t tid1 ; //, tid2;

	// set time
	s_startTime = gettimeofdayInMilliseconds_force();

	int32_t stksize = 1000000 ;
	int32_t bufsize = stksize * s_numThreads ;
#undef malloc
	char *buf = (char *)malloc ( bufsize );
#define malloc coreme
	if ( ! buf ) { log("test: malloc of %"INT32" failed.",bufsize); return; }
	g_conf.m_useThreads = true;
	//int pid;
	for ( int32_t i = 0 ; i < s_numThreads ; i++ ) {
		//int err = pthread_create ( &tid1,&s_attr,startUp,(void *)i) ;
		if (!g_threads.call(GENERIC_THREAD,0,
				    (void *)(PTRTYPE)i,NULL,startUp)){
			log("test: Thread launch failed."); return; }
		//pid = clone ( startUp , buf + stksize * i ,
		//      CLONE_FS | CLONE_FILES | CLONE_VM | //CLONE_SIGHAND |
		//		  SIGCHLD ,
		//		  (void *)NULL );
		//if ( pid == (pid_t)-1) {log("test: error cloning"); return;}
		//log(LOG_INIT,"test:launched i=%"INT32" pid=%i",i,(int)pid);
		//log(LOG_INIT,"test:launched i=%"INT32"",i,(int)pid);
		log(LOG_INIT,"test: Launched thread #%"INT32".",i);
		//if ( err != 0     ) return ;// -1;
	}
	// unset lock
	//s_lock = 0;
	// sleep til done
#undef sleep
	while ( 1 == 1 ) sleep(1000);
#define sleep(a) { char *xx=NULL;*xx=0; }
	//int status;
	//for ( int32_t i = 0 ; i < s_numThreads ; i++ ) waitpid(pid,&status,0);
}

//int startUp ( void *state ) {
void *startUp ( void *state , ThreadEntry *t ) {
	int32_t id = (int32_t) (PTRTYPE)state;
	// . what this lwp's priority be?
	// . can range from -20 to +20
	// . the lower p, the more cpu time it gets
	// . this is really the niceness, not the priority
	//int p = 0;
	//if ( id == 1 ) p = 0;
	//else           p = 30;
	// . set this process's priority
	// . setpriority() is only used for SCHED_OTHER threads
	//if ( setpriority ( PRIO_PROCESS, getpid() , p ) < 0 ) {
	//	fprintf(stderr,"Threads::startUp: setpriority: failed\n");
	//	exit(-1);
	//}
	// read buf
	//char buf [ MAX_READ_SIZE ];
#undef malloc
	char *buf = (char *) malloc ( s_maxReadSize );
#define malloc coreme
	if ( ! buf ) { 
		fprintf(stderr,"MALLOC FAILED in thread\n");
		return 0; // NULL;
	}
	// we got ourselves
	s_launched++;
	// msg
	fprintf(stderr,"id=%"INT32" launched. Performing 100000 reads.\n",id);
	// wait for lock to be unleashed
	//while ( s_launched != s_numThreads ) usleep(10);
	// now do a stupid loop
	//int32_t j, off , size;
	int64_t off , size;
	for ( int32_t i = 0 ; i < 100000 ; i++ ) {
		uint64_t r = rand();
		r <<= 32 ;
		r |= rand();
		off = r % (s_filesize - s_maxReadSize );
		// rand size
		//size = rand() % s_maxReadSize;
		size = s_maxReadSize;
		//if ( size < 32*1024 ) size = 32*1024;
		// time it
		int64_t start = gettimeofdayInMilliseconds_force();
		//fprintf(stderr,"%"INT32") i=%"INT32" start\n",id,i );
		//pread ( s_fd1 , buf , size , off );
		s_f.read ( buf , size , off );
		//fprintf(stderr,"%"INT32") i=%"INT32" done\n",id,i );
		int64_t now = gettimeofdayInMilliseconds_force();
#undef usleep
		usleep(0);
#define usleep(a) { char *xx=NULL;*xx=0; }
		s_count++;
		float sps = (float)((float)s_count * 1000.0) / 
			(float)(now - s_startTime);
		fprintf(stderr,"count=%"INT32" off=%012"INT64" size=%"INT32" time=%"INT32"ms "
			"(%.2f seeks/sec)\n",
			(int32_t)s_count,
			(int64_t)off,
			(int32_t)size,
			(int32_t)(now - start) , 
			sps );
	}
		

	// dummy return
	return 0; //NULL;
}

void dumpTagdb( char *coll, int32_t startFileNum, int32_t numFiles, bool includeTree, char req, int32_t rdbId,
				char *siteArg ) {
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	g_tagdb.init ();
	//g_collectiondb.init(true);

	if ( rdbId == RDB_TAGDB ) {
		g_tagdb.getRdb()->addRdbBase1(coll );
	}

	key128_t startKey ;
	key128_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	if ( siteArg ) {
		startKey = g_tagdb.makeStartKey ( siteArg );
		endKey = g_tagdb.makeEndKey ( siteArg );
		log("gb: using site %s for start key",siteArg );
	}

	// turn off threads
	g_threads.disableThreads();

	// get a meg at a time
	int32_t minRecSizes = 1024*1024;
	Msg5 msg5;
	RdbList list;

	CollectionRec *cr = g_collectiondb.getRec(coll);

	int64_t hostHash = -1;
	int64_t lastHostHash = -2;
	char *site = NULL;
	char sbuf[1024*2];
	int32_t siteNumInlinks = -1;
	int32_t typeSite = hash64Lower_a("site",4);
	int32_t typeInlinks = hash64Lower_a("sitenuminlinks",14);

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( rdbId,
			      cr->m_collnum      ,
			      &list         ,
			      (char *)&startKey      ,
			      (char *)&endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// loop over entries in list
	for(list.resetListPtr();!list.isExhausted(); list.skipCurrentRecord()){
		char *rec  = list.getCurrentRec();
		//key_t k    = list.getCurrentKey();
		key128_t k;
		list.getCurrentKey ( &k );
		char *data = list.getCurrentData();
		int32_t  size = list.getCurrentDataSize();
		// is it a delete?
		if ( (k.n0 & 0x01) == 0 ) {
			if ( req == 'z' ) continue;
			printf("k.n1=%016"XINT64" "
			       "k.n0=%016"XINT64" (delete)\n",
			       k.n1  , k.n0   | 0x01  );  // fix it!
			continue;
		}
		// point to the data
		char  *p       = data;
		char  *pend    = data + size;
		// breach check
		if ( p >= pend ) {
			printf("corrupt tagdb rec k.n0=%"UINT64"",k.n0);
			continue;
		}

		// parse it up
		Tag *tag = (Tag *)rec;

		// print the version and site
		char tmpBuf[1024];
		SafeBuf sb(tmpBuf, 1024);

		bool match = false;

		hostHash = tag->m_key.n1;

		if ( hostHash == lastHostHash ) {
			match = true;
		}
		else {
			site = NULL;
			siteNumInlinks = -1;
		}

		lastHostHash = hostHash;

		// making sitelist.txt?
		if ( tag->m_type == typeSite && req == 'z' ) {
			site = tag->getTagData();
			// make it null if too many .'s
			if ( site ) {
				char *p = site;
				int count = 0;
				int alpha = 0;
				int colons = 0;
				// foo.bar.baz.com is ok
				for ( ; *p ; p++ ) {
					if ( *p == '.' ) count++;
					if ( *p == ':' ) colons++;
					if ( is_alpha_a(*p) || *p=='-' ) 
						alpha++;
				}
				if ( count >= 4 )
					site = NULL;
				if ( colons > 1 )
					site = NULL;
				// no ip addresses allowed, need an alpha char
				if ( alpha == 0 )
					site = NULL;
			}
			// ends in :?
			int slen = 0;
			if ( site ) slen = gbstrlen(site);
			if ( site && site[slen-1] == ':' )
				site = NULL;
			// port bug
			if ( site && site[slen-2] == ':' && site[slen-1]=='/')
				site = NULL;
			// remove heavy spammers to save space
			if ( site && strstr(site,"daily-camshow-report") )
				site = NULL;
			if ( site && strstr(site,".livejasminhd.") )
				site = NULL;
			if ( site && strstr(site,".pornlivenews.") )
				site = NULL;
			if ( site && strstr(site,".isapornblog.") )
				site = NULL;
			if ( site && strstr(site,".teen-model-24.") )
				site = NULL;
			if ( site && ! is_ascii2_a ( site, gbstrlen(site) ) ) {
				site = NULL;
				continue;
			}
			if ( match && siteNumInlinks>=0) {
				// if we ask for 1 or 2 we end up with 100M
				// entries, but with 3+ we get 27M
				if ( siteNumInlinks > 2 && site )
					printf("%i %s\n",siteNumInlinks,site);
				siteNumInlinks = -1;
				site = NULL;
			}
			// save it
			if ( site ) strcpy ( sbuf , site );
			continue;
		}

		if ( tag->m_type == typeInlinks && req == 'z' ) {
			siteNumInlinks = atoi(tag->getTagData());
			if ( match && site ) {
				// if we ask for 1 or 2 we end up with 100M
				// entries, but with 3+ we get 27M
				if ( siteNumInlinks > 2 )
					printf("%i %s\n",siteNumInlinks,sbuf);
				siteNumInlinks = -1;
				site = NULL;
			}
			continue;
		}

		if ( req == 'z' )
			continue;

		// print as an add request or just normal
		if ( req == 'A' ) tag->printToBufAsAddRequest ( &sb );
		else              tag->printToBuf             ( &sb );

		// dump it
		printf("%s\n",sb.getBufStart());

	}
		
	startKey = *(key128_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key128_t *)list.getLastKey() ){ 
		printf("\n"); return;}
	goto loop;
}

bool parseTest ( char *coll , int64_t docId , char *query ) {
	g_conf.m_maxMem = 2000000000LL; // 2G
	//g_mem.m_maxMem  = 2000000000LL; // 2G
	//g_conf.m_spiderdbMaxDiskPageCacheMem   = 0;
	//g_conf.m_titledbMaxTreeMem = 1024*1024*10;
	g_titledb.init ();
	//g_collectiondb.init(true);
	g_titledb.getRdb()->addRdbBase1 ( coll );
	log(LOG_INIT, "build: Testing parse speed of html docId %"INT64".",docId);
	// get a title rec
	g_threads.disableThreads();
	RdbList tlist;
	key_t startKey = g_titledb.makeFirstKey ( docId );
	key_t endKey   = g_titledb.makeLastKey  ( docId );
	// a niceness of 0 tells it to block until it gets results!!
	Msg5 msg5;
	Msg5 msg5b;

	CollectionRec *cr = g_collectiondb.getRec(coll);
	if ( ! msg5.getList ( RDB_TITLEDB    ,
			      cr->m_collnum        ,
			      &tlist         ,
			      startKey       ,
			      endKey         , // should be maxed!
			      9999999        , // min rec sizes
			      true           , // include tree?
			      false          , // includeCache
			      false          , // addToCache
			      0              , // startFileNum
			      -1             , // m_numFiles   
			      NULL           , // state 
			      NULL           , // callback
			      0              , // niceness
			      false          , // do error correction?
			      NULL           , // cache key ptr
			      0              , // retry num
			      -1             , // maxRetries
			      true           , // compensate for merge
			      -1LL           , // sync point
			      &msg5b         ))
		return log(LOG_LOGIC,"build: getList did not block.");
	// get the title rec
	if ( tlist.isEmpty() ) 
		return log("build: speedtestxml: "
			   "docId %"INT64" not found.", 
			   docId );
	if (!ucInit(g_hostdb.m_dir))
		return log("Unicode initialization failed!");

	// get raw rec from list
	char *rec      = tlist.getCurrentRec();
	int32_t  listSize = tlist.getListSize ();
	// set the titleRec we got
	//TitleRec tr ;
	//if ( ! tr.set ( rec , listSize , false /*own data?*/ ) ) 
	//	return log("build: speedtestxml: Error setting "
	//		   "titleRec." );
	XmlDoc xd;
	if ( ! xd.set2 ( rec , listSize , coll , NULL , 0 ) )
		return log("build: speedtestxml: Error setting "
			   "xml doc." );
	log("build: Doc url is %s",xd.ptr_firstUrl);//tr.getUrl()->getUrl());
	log("build: Doc is %"INT32" bytes long.",xd.size_utf8Content-1);
	log("build: Doc charset is %s",get_charset_str(xd.m_charset));


	// time the summary/title generation code
	log("build: Using query %s",query);
	summaryTest1   ( rec , listSize , coll , docId , query );
	//summaryTest2   ( rec , listSize , coll , docId , query );
	//summaryTest3   ( rec , listSize , coll , docId , query );

	// for a 128k latin1 doc: (access time is probably 15-20ms)
	// 1.18 ms to set title rec (6ms total)
	// 1.58 ms to set Xml
	// 1.71 ms to set Words (~50% from Words::countWords())
	// 0.42 ms to set Pos
	// 0.66 ms to set Bits
	// 0.51 ms to set Scores
	// 0.35 ms to getText()

	// speed test
	int64_t t = gettimeofdayInMilliseconds_force();
	for ( int32_t k = 0 ; k < 100 ; k++ )
		xd.set2 (rec, listSize, coll , NULL , 0 );
	int64_t e = gettimeofdayInMilliseconds_force();
	logf(LOG_DEBUG,"build: Took %.3f ms to set title rec.",
	     (float)(e-t)/100.0);

	// speed test
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t k = 0 ; k < 100 ; k++ ) {
		char *mm = (char *)mmalloc ( 300*1024 , "ztest");
		mfree ( mm , 300*1024 ,"ztest");
	}
	e = gettimeofdayInMilliseconds_force();
	logf(LOG_DEBUG,"build: Took %.3f ms to do mallocs.",
	     (float)(e-t)/100.0);

	// get content
	char *content    = xd.ptr_utf8Content;//tr.getContent();
	int32_t  contentLen = xd.size_utf8Content-1;//tr.getContentLen();

	// loop parse
	Xml xml;
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) {
		if ( !xml.set( content, contentLen, xd.m_version, 0, CT_HTML ) ) {
			return log("build: speedtestxml: xml set: %s",
				   mstrerror(g_errno));
		}
	}

	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Xml::set() took %.3f ms to parse docId %"INT64".", 
	    (double)(e - t)/100.0,docId);
	double bpms = contentLen/((double)(e-t)/100.0);
	log("build: %.3f bytes/msec", bpms);
	// get per char and per byte speeds
	xml.reset();

	// loop parse
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) {
		if ( !xml.set( content, contentLen, xd.m_version, 0, CT_HTML ) ) {
			return log("build: xml(setparents=false): %s",
				   mstrerror(g_errno));
		}
	}

	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Xml::set(setparents=false) took %.3f ms to "
	    "parse docId %"INT64".", (double)(e - t)/100.0,docId);


	if (!ucInit(g_hostdb.m_dir)) {
		log("Unicode initialization failed!");
		return 1;
	}
	Words words;

	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) 
		if ( ! words.set ( &xml , true , true ) )
			return log("build: speedtestxml: words set: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Words::set(xml,computeIds=true) took %.3f ms for %"INT32" words"
	    " (precount=%"INT32") for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,words.m_preCount,docId);


	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) 
		if ( ! words.set ( &xml , true , false ) )
			return log("build: speedtestxml: words set: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Words::set(xml,computeIds=false) "
	    "took %.3f ms for %"INT32" words"
	    " (precount=%"INT32") for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,words.m_preCount,docId);


	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) 
		//if ( ! words.set ( &xml , true , true ) )
		if ( ! words.set ( content ,
				   true, 0 ) )
			return log("build: speedtestxml: words set: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Words::set(content,computeIds=true) "
	    "took %.3f ms for %"INT32" words "
	    "for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,docId);


	Pos pos;
	// computeWordIds from xml
	words.set ( &xml , true , true ) ;
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) 
		//if ( ! words.set ( &xml , true , true ) )
		if ( ! pos.set ( &words ) )
			return log("build: speedtestxml: pos set: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Pos::set() "
	    "took %.3f ms for %"INT32" words "
	    "for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,docId);


	Bits bits;
	// computeWordIds from xml
	words.set ( &xml , true , true ) ;
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) 
		//if ( ! words.set ( &xml , true , true ) )
		if ( ! bits.setForSummary ( &words ) )
			return log("build: speedtestxml: Bits set: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Bits::setForSummary() "
	    "took %.3f ms for %"INT32" words "
	    "for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,docId);


	Sections sections;
	// computeWordIds from xml
	words.set ( &xml , true , true ) ;
	bits.set ( &words ,TITLEREC_CURRENT_VERSION, 0);
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) 
		//if ( ! words.set ( &xml , true , true ) )
		// do not supply xd so it will be set from scratch
		if ( !sections.set( &words, &bits, NULL, NULL, 0, 0 ) )
			return log("build: speedtestxml: sections set: %s",
				   mstrerror(g_errno));

	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Scores::set() "
	    "took %.3f ms for %"INT32" words "
	    "for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,docId);

	

	//Phrases phrases;
	Phrases phrases;
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ )
		if ( !phrases.set( &words, &bits, 0 ) )
			return log("build: speedtestxml: Phrases set: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Phrases::set() "
	    "took %.3f ms for %"INT32" words "
	    "for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,docId);



	bool isPreformattedText ;
	int32_t contentType = xd.m_contentType;//tr.getContentType();
	if ( contentType == CT_TEXT ) isPreformattedText = true;
	else                          isPreformattedText = false;


	char *buf = (char *)mmalloc(contentLen*2+1,"main");
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ )
		if ( !xml.getText( buf, contentLen * 2 + 1, 0, 9999999, true ) )
			return log("build: speedtestxml: getText: %s",
				   mstrerror(g_errno));
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Xml::getText(computeIds=false) took %.3f ms for docId "
	    "%"INT64".",(double)(e - t)/100.0,docId);



	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) {
		int32_t bufLen = xml.getText( buf, contentLen * 2 + 1, 0, 9999999, true );
		if ( ! bufLen ) return log("build: speedtestxml: getText: %s",
					   mstrerror(g_errno));
		if ( ! words.set ( buf,true,0) )
			return log("build: speedtestxml: words set: %s",
				   mstrerror(g_errno));
	}

	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Xml::getText(computeIds=false) w/ word::set() "
	    "took %.3f ms for docId "
	    "%"INT64".",(double)(e - t)/100.0,docId);



	Matches matches;
	Query q;
	//int32_t collLen = gbstrlen(coll);
	q.set2 ( query , langUnknown , false );
	matches.setQuery ( &q );
	words.set ( &xml , true , 0 ) ;
	t = gettimeofdayInMilliseconds_force();
	for ( int32_t i = 0 ; i < 100 ; i++ ) {
		matches.reset();
		if ( ! matches.addMatches ( &words ) )
			return log("build: speedtestxml: matches set: %s",
				   mstrerror(g_errno));
	}
	// print time it took
	e = gettimeofdayInMilliseconds_force();
	log("build: Matches::set() took %.3f ms for %"INT32" words"
	    " (precount=%"INT32") for docId %"INT64".", 
	    (double)(e - t)/100.0,words.m_numWords,words.m_preCount,docId);



	return true;
}	

bool summaryTest1   ( char *rec , int32_t listSize, char *coll , int64_t docId , char *query ) {

	// start the timer
	int64_t t = gettimeofdayInMilliseconds_force();

	Query q;
	q.set2 ( query , langUnknown , false );

	char *content ;
	int32_t  contentLen ;

	// loop parse
	for ( int32_t i = 0 ; i < 100 ; i++ ) {
		XmlDoc xd;
		xd.set2 (rec, listSize, coll,NULL,0);
		// get content
		content    = xd.ptr_utf8Content;//tr.getContent();
		contentLen = xd.size_utf8Content-1;//tr.getContentLen();

		// now parse into xhtml (takes 15ms on lenny)
		Xml xml;
		xml.set( content, contentLen, xd.m_version, 0, CT_HTML );

		xd.getSummary();
	}

	// print time it took
	int64_t e = gettimeofdayInMilliseconds_force();
	log("build: V1  Summary/Title/Gigabits generation took %.3f ms for docId "
	    "%"INT64".", 
	    (double)(e - t)/100.0,docId);
	double bpms = contentLen/((double)(e-t)/100.0);
	log("build: %.3f bytes/msec", bpms);
	return true;
}

// mostly taken from Msg20.cpp
/*
bool summaryTest2   ( char *rec , int32_t listSize, char *coll , int64_t docId ,
		      char *query ) {

	//int32_t collLen = gbstrlen(coll);
	CollectionRec *cr = g_collectiondb.getRec ( coll );

	// start the timer
	int64_t t = gettimeofdayInMilliseconds_force();

	int32_t titleMaxLen               = cr->m_titleMaxLen;
	int32_t summaryMaxLen             = cr->m_summaryMaxLen;
	int32_t numSummaryLines           = cr->m_summaryMaxNumLines;
	int32_t summaryMaxNumCharsPerLine = cr->m_summaryMaxNumCharsPerLine;
	// these are arbitrary (taken from Msg24.cpp)
	bool ratInSummary              = false;

	Query q;
	q.set ( query , 0 ); // boolFlag

	char *content ;
	int32_t  contentLen ;

	// loop parse
	for ( int32_t i = 0 ; i < 100 ; i++ ) {

		// 4ms
		TitleRec tr;
		tr.set (rec, listSize, false);
		// get content
		content    = tr.getContent();
		contentLen = tr.getContentLen();

		// time it
		//logf(LOG_TIMING,"query: summary step 1");
		// now parse into xhtml (takes 15ms on lenny)
		// 1ms
		Xml xml;
		xml.set ( tr.getCharset() , content, contentLen , 
			  false, 0, false, tr.getVersion() );
		// time it
		//logf(LOG_TIMING,"query: summary step 2");
		// 7ms
		Words ww;
		ww.set ( &xml ,
			 true , // compute word ids?
			 true );// has html entities?

		// time it
		// 0ms
		//logf(LOG_TIMING,"query: summary step 3");
		//int32_t  sfn = tr.getSiteFilenum();
		//Xml  *sx  = g_tagdb.getSiteXml ( sfn , coll , collLen );
		// time it
		//logf(LOG_TIMING,"query: summary step 4");
		// 5ms
		Sections ss;
		ss.set ( &ww ,NULL,0,NULL,NULL,&tr);

		// time it
		//logf(LOG_TIMING,"query: summary step 5");

		// 3.5ms
		Pos pos;
		pos.set ( &ww , &ss );

		// time it
		//logf(LOG_TIMING,"query: summary step 6");

		// .5ms
		Title tt;
		// use hard title? false!
		tt.setTitle(&tr,&xml,&ww,&ss,&pos,titleMaxLen,0xffff, NULL);
		char *tbuf    = tt.getTitle();
		int32_t  tbufLen = tt.m_titleBytes;
		// sanity check
		if ( ! tbuf && tbufLen ) { char *xx = NULL; *xx = 0; }

		// time it
		//logf(LOG_TIMING,"query: summary step 7");
		// 1ms
		Bits bb;
		if ( ! bb.setForSummary ( &ww ) ) return false;
		// time it
		//logf(LOG_TIMING,"query: summary step 8");

		// 8-9ms
		Summary s;
		bool status;
		status = s.set2 ( &xml                      , 
				  &ww                       ,
				  &bb                       ,
				  &ss                       ,
				  &pos                      ,
				  &q                        ,
				  NULL                      , // termFreqs
				  NULL                      , // affWeights
				  coll                      ,
				  collLen                   ,
				  false                     , // doStemming? 
				  summaryMaxLen             ,
				  numSummaryLines           ,
				  summaryMaxNumCharsPerLine ,
				  ratInSummary              ,
				  &tr                       );
		// time it
		//logf(LOG_TIMING,"query: summary step 9");
	}

	// print time it took
	int64_t e = gettimeofdayInMilliseconds_force();
	log("build: V2  Summary/Title/Gigabits generation took %.3f ms for "
	    "docId %"INT64".",
	    (double)(e - t)/100.0,docId);
	double bpms = contentLen/((double)(e-t)/100.0);
	log("build: %.3f bytes/msec", bpms);
	return true;
}

*/

void dumpIndexdbFile ( int32_t fn , int64_t off , char *ff , int32_t ks ,
		       char *NAME ) {
	// this is confidential data format
#ifdef _CLIENT_
	return;
#endif
	char buf [ 1000000 ];
	int32_t bufSize = 1000000;
	char fname[64];
	sprintf ( fname , "%s%04"INT32".dat" , ff,fn );
	if ( NAME ) sprintf ( fname , "%s", NAME );
	BigFile f;
	fprintf(stderr,"opening ./%s\n",fname);
	f.set ( "./" , fname );
	if ( ! f.open ( O_RDONLY ) ) return;
	// init our vars
	bool haveTop = false;
	char top[6];
	memset ( top , 0 , 6 );
	bool warned = false;
	// how big is this guy?
	int64_t filesize = f.getFileSize();
	fprintf(stderr,"filesize=%"INT64"\n",filesize);
	fprintf(stderr,"off=%"INT64"\n",off);
	// reset error number
	g_errno = 0;
	// the big read loop
 loop:
	int64_t readSize = bufSize;
	if ( off + readSize > filesize ) readSize = filesize - off;
	// return if we're done reading the whole file
	if ( readSize <= 0 ) return;
	// read in as much as we can
	f.read ( buf , readSize , off );
	// bail on read error
	if ( g_errno ) {
		fprintf(stderr,"read of %s failed",f.getFilename());
		return;
	}
	char *p    = buf;
	char *pend = buf + readSize;
 inner:
	// parse out the keys
	int32_t size;
	if ( ((*p) & 0x02) == 0x00 ) size = ks;
	else                         size = ks-6;
	if ( p + size > pend ) {
		// skip what we read
		off  += readSize ;
		// back up so we don't split a key we should not
		off -= ( pend - p );
		// read more
		goto loop;
	}
	// new top?
	if ( size == ks ) { gbmemcpy ( top , p + (ks-6) , 6 ); haveTop = true; }
	// warning msg
	if ( ! haveTop && ! warned ) {
		warned = true;
		log("db: Warning: first key is a half key.");
	}
	// make the key
	char tmp [ MAX_KEY_BYTES ];
	gbmemcpy ( tmp , p , ks-6 );
	gbmemcpy ( tmp + ks-6 , top , 6 );
	// print the key
	if ( ks == 12 )
		fprintf(stdout,"%08"INT64") %08"XINT32" %016"XINT64"\n",
			off + (p - buf) ,
			*(int32_t *)(tmp+8),*(int64_t *)tmp );
	else
		fprintf(stdout,"%08"INT64") %016"XINT64" %016"XINT64"\n",
			off + (p - buf) ,
			*(int64_t *)(tmp+8),*(int64_t *)tmp );
	// go to next key
	p += size;
	// loop up
	goto inner;
}

void dumpIndexdb (char *coll,int32_t startFileNum,int32_t numFiles,bool includeTree, 
		   int64_t termId ) {
	// this is confidential data format
#ifdef _CLIENT_
	return;
#endif
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	g_indexdb.init ();
	//g_collectiondb.init(true);
	g_indexdb.getRdb()->addRdbBase1(coll );
	key_t startKey ;
	key_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	if ( termId >= 0 ) {
		startKey = g_indexdb.makeFirstKey ( termId );
		endKey   = g_indexdb.makeLastKey  ( termId );
	}
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;

	// bail if not
	if ( g_indexdb.m_rdb.getNumFiles() <= startFileNum && numFiles > 0 ) {
		printf("Request file #%"INT32" but there are only %"INT32" "
		       "indexdb files\n",startFileNum,
		       g_indexdb.m_rdb.getNumFiles());
		return;
	}

	Msg5 msg5;
	RdbList list;
	CollectionRec *cr = g_collectiondb.getRec(coll);

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_INDEXDB   ,
			      cr->m_collnum      ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {

		key_t k    = list.getCurrentKey();
		// is it a delete?
		char *dd = "";
		if ( (k.n0 & 0x01) == 0x00 ) dd = " (delete)";
		int64_t d = g_indexdb.getDocId(k);
		uint8_t dh = g_titledb.getDomHash8FromDocId(d);
		if ( termId < 0 )
			printf("k.n1=%08"XINT32" k.n0=%016"XINT64" "
			       "tid=%015"UINT64" score=%03"INT32" "
			       "docId=%012"INT64" dh=0x%02"XINT32"%s\n" , 
			       k.n1, k.n0, (int64_t)g_indexdb.getTermId(k),
			       (int32_t)g_indexdb.getScore(k) ,
			       d , (int32_t)dh, dd );
		else
			printf("k.n1=%08"XINT32" k.n0=%016"XINT64" "
			       "score=%03"INT32" docId=%012"INT64" dh=0x%02"XINT32"%s\n" , 
			       k.n1, k.n0,
			       (int32_t)g_indexdb.getScore(k) ,
			       d , (int32_t)dh, dd );

		continue;
	}

	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key_t *)list.getLastKey() ) return;
	goto loop;
}

void dumpPosdb (char *coll,int32_t startFileNum,int32_t numFiles,bool includeTree, 
		   int64_t termId , bool justVerify ) {
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;

	if ( ! justVerify ) {
		g_posdb.init ();
		//g_collectiondb.init(true);
		g_posdb.getRdb()->addRdbBase1(coll );
	}

	key144_t startKey ;
	key144_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	if ( termId >= 0 ) {
		g_posdb.makeStartKey ( &startKey, termId );
		g_posdb.makeEndKey  ( &endKey, termId );
		printf("termid=%"UINT64"\n",termId);
		printf("startkey=%s\n",KEYSTR(&startKey,sizeof(POSDBKEY)));
		printf("endkey=%s\n",KEYSTR(&endKey,sizeof(POSDBKEY)));
	}
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;

	// bail if not
	if ( g_posdb.m_rdb.getNumFiles() <= startFileNum && numFiles > 0 ) {
		printf("Request file #%"INT32" but there are only %"INT32" "
		       "posdb files\n",startFileNum,
		       g_posdb.m_rdb.getNumFiles());
		return;
	}

	key144_t lastKey;
	lastKey.setMin();

	Msg5 msg5;
	RdbList list;

	// set this flag so Msg5.cpp if it does error correction does not
	// try to get the list from a twin...
	g_isDumpingRdbFromMain = 1;
	CollectionRec *cr = g_collectiondb.getRec(coll);

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_POSDB   ,
			      cr->m_collnum      ,
			      &list         ,
			      &startKey      ,
			      &endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      true )) { // to debug RdbList::removeBadData_r()
		            //false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;

	// get last key in list
	char *ek2 = list.m_endKey;
	// print it
	printf("ek=%s\n",KEYSTR(ek2,list.m_ks) );

	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() && ! justVerify ;
	      list.skipCurrentRecord() ) {
		key144_t k; list.getCurrentKey(&k);
		// compare to last
		char *err = "";
		if ( KEYCMP((char *)&k,(char *)&lastKey,sizeof(key144_t))<0 ) 
			err = " (out of order)";
		lastKey = k;
		// is it a delete?
		char *dd = "";
		if ( (k.n0 & 0x01) == 0x00 ) dd = " (delete)";
		int64_t d = g_posdb.getDocId(&k);
		uint8_t dh = g_titledb.getDomHash8FromDocId(d);
		char *rec = list.m_listPtr;
		int32_t recSize = 18;
		if ( rec[0] & 0x04 ) recSize = 6;
		else if ( rec[0] & 0x02 ) recSize = 12;
		// alignment bits check
		if ( recSize == 6  && !(rec[1] & 0x02) ) {
			int64_t nd1 = g_posdb.getDocId(rec+6);
			// seems like nd2 is it, so it really is 12 bytes but
			// does not have the alignment bit set...
			//int64_t nd2 = g_posdb.getDocId(rec+12);
			//int64_t nd3 = g_posdb.getDocId(rec+18);
			// what size is it really?
			// seems like 12 bytes
			//log("debug1: d=%"INT64" nd1=%"INT64" nd2=%"INT64" nd3=%"INT64"",
			//d,nd1,nd2,nd3);
			err = " (alignerror1)";
			if ( nd1 < d ) err = " (alignordererror1)";
			//char *xx=NULL;*xx=0;
		}
		if ( recSize == 12 && !(rec[1] & 0x02) )  {
			//int64_t nd1 = g_posdb.getDocId(rec+6);
			// seems like nd2 is it, so it really is 12 bytes but
			// does not have the alignment bit set...
			int64_t nd2 = g_posdb.getDocId(rec+12);
			//int64_t nd3 = g_posdb.getDocId(rec+18);
			// what size is it really?
			// seems like 12 bytes
			//log("debug1: d=%"INT64" nd1=%"INT64" nd2=%"INT64" nd3=%"INT64"",
			//d,nd1,nd2,nd3);
			//if ( nd2 < d ) { char *xx=NULL;*xx=0; }
			//char *xx=NULL;*xx=0;
			err = " (alignerror2)";
			if ( nd2 < d ) err = " (alignorderrror2)";
		}
		// if it 
		if ( recSize == 12 &&  (rec[7] & 0x02)) { 
			//int64_t nd1 = g_posdb.getDocId(rec+6);
			// seems like nd2 is it, so it really is 12 bytes but
			// does not have the alignment bit set...
			int64_t nd2 = g_posdb.getDocId(rec+12);
			//int64_t nd3 = g_posdb.getDocId(rec+18);
			// what size is it really?
			// seems like 12 bytes really as well!
			//log("debug2: d=%"INT64" nd1=%"INT64" nd2=%"INT64" nd3=%"INT64"",
			//d,nd1,nd2,nd3);
			//char *xx=NULL;*xx=0;
			err = " (alignerror3)";
			if ( nd2 < d ) err = " (alignordererror3)";
		}
		if ( KEYCMP((char *)&k,(char *)&startKey,list.m_ks)<0 || 
		     KEYCMP((char *)&k,ek2,list.m_ks)>0){
			err = " (out of range)";
		}

		if ( termId < 0 )
			printf(
			       "k=%s "
			       "tid=%015"UINT64" "
			       "docId=%012"INT64" "

			       "siterank=%02"INT32" "
			       "langid=%02"INT32" "
			       "pos=%06"INT32" "
			       "hgrp=%02"INT32" "
			       "spamrank=%02"INT32" "
			       "divrank=%02"INT32" "
			       "syn=%01"INT32" "
			       "densrank=%02"INT32" "
			       "mult=%02"INT32" "

			       "dh=0x%02"XINT32" "
			       "rs=%"INT32"" //recSize
			       "%s" // dd
			       "%s" // err
			       "\n" , 
			       KEYSTR(&k,sizeof(key144_t)),
			       (int64_t)g_posdb.getTermId(&k),
			       d , 
			       (int32_t)g_posdb.getSiteRank(&k),
			       (int32_t)g_posdb.getLangId(&k),
			       (int32_t)g_posdb.getWordPos(&k),
			       (int32_t)g_posdb.getHashGroup(&k),
			       (int32_t)g_posdb.getWordSpamRank(&k),
			       (int32_t)g_posdb.getDiversityRank(&k),
			       (int32_t)g_posdb.getIsSynonym(&k),
			       (int32_t)g_posdb.getDensityRank(&k),
			       (int32_t)g_posdb.getMultiplier(&k),
			       
			       (int32_t)dh, 
			       recSize,
			       dd ,
			       err );
		else
			printf(
			       "k=%s "
			       "tid=%015"UINT64" "
			       "docId=%012"INT64" "
			       "siterank=%02"INT32" "
			       "langid=%02"INT32" "
			       "pos=%06"INT32" "
			       "hgrp=%02"INT32" "
			       "spamrank=%02"INT32" "
			       "divrank=%02"INT32" "
			       "syn=%01"INT32" "
			       "densrank=%02"INT32" "
			       "mult=%02"INT32" "
			       "recSize=%"INT32" "
			       "dh=0x%02"XINT32"%s%s\n" , 
			       KEYSTR(&k,sizeof(key144_t)),
			       (int64_t)g_posdb.getTermId(&k),
			       d , 
			       (int32_t)g_posdb.getSiteRank(&k),
			       (int32_t)g_posdb.getLangId(&k),
			       (int32_t)g_posdb.getWordPos(&k),
			       (int32_t)g_posdb.getHashGroup(&k),
			       (int32_t)g_posdb.getWordSpamRank(&k),
			       (int32_t)g_posdb.getDiversityRank(&k),
			       (int32_t)g_posdb.getIsSynonym(&k),
			       (int32_t)g_posdb.getDensityRank(&k),
			       (int32_t)g_posdb.getMultiplier(&k),
			       recSize,
			       
			       (int32_t)dh, 
			       dd ,
			       err );
		continue;
	}

	startKey = *(key144_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key144_t *)list.getLastKey() ) return;
	goto loop;
}

void dumpDatedb (char *coll,int32_t startFileNum,int32_t numFiles,bool includeTree, 
		 int64_t termId , bool justVerify ) {
	// this is confidential data format
#ifdef _CLIENT_
	return;
#endif
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	if ( ! justVerify ) {
		g_datedb.init ();
		//g_collectiondb.init(true);
		g_datedb.getRdb()->addRdbBase1(coll );
	}
	char startKey[16];
	char endKey  [16];
	int64_t termId1 = 0x0000000000000000LL;
	int64_t termId2 = 0xffffffffffffffffLL;
	if ( termId >= 0 ) {
		termId1 = termId;
		termId2 = termId;
	}
	key128_t kk;
	kk = g_datedb.makeStartKey ( termId1 , 0xffffffff );

	// tmp hack
	//kk.n1 = 0x51064d5bdd71bd51LL;
	//kk.n0 = 0x649ffe3f20f617c6LL;

	KEYSET(startKey,(char *)&kk,16);
	kk = g_datedb.makeEndKey   ( termId2 , 0x00000000 );
	KEYSET(endKey,(char *)&kk,16);
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;

	// bail if not
	if ( g_datedb.m_rdb.getNumFiles() <= startFileNum ) {
		printf("Request file #%"INT32" but there are only %"INT32" "
		       "datedb files\n",startFileNum,
		       g_datedb.m_rdb.getNumFiles());
		//return;
	}
	// turn off threads
	g_threads.disableThreads();

	Msg5 msg5;
	IndexList list;
	CollectionRec *cr = g_collectiondb.getRec(coll);
 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_DATEDB ,
			      cr->m_collnum          ,
			      &list         ,
			      (char *)&startKey      ,
			      (char *)&endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		g_threads.enableThreads();
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) {
		g_threads.enableThreads();
		return;
	}
	uint8_t a,b;
	int64_t lattid  = hash64n("gbxlatitude") & TERMID_MASK;
	int64_t lontid  = hash64n("gbxlongitude")& TERMID_MASK;
	//int64_t lattid2 = hash64n("gbxlatitudecity") & TERMID_MASK;
	int64_t lattid2 = hash64n("gbxlatitude2") & TERMID_MASK;
	//int64_t lontid2 = hash64n("gbxlongitudecity")& TERMID_MASK;
	int64_t lontid2 = hash64n("gbxlongitude2")& TERMID_MASK;
	int64_t starttid= hash64n("gbxstart")& TERMID_MASK;
	int64_t endtid  = hash64n("gbxend")& TERMID_MASK;
	// sanity check
	if ( list.m_ks != 16 ) { char *xx = NULL; *xx = 0; }
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() && ! justVerify ;
	      list.skipCurrentRecord() ) {
		//key_t k    = list.getCurrentKey();
		uint8_t k[MAX_KEY_BYTES];
		list.getCurrentKey(k);
		// is it a delete?
		char *dd = "";
		//if ( (k.n0 & 0x01) == 0x00 ) dd = " (delete)";
		if ( KEYNEG((char *)k) ) dd = " (delete)";

		// get event id range
		a = 255 - k[7];
		b = 255 - k[6];

		// hack flag for indexing tag terms (complemented)
		bool isTagTerm = (k[9] == 0x7f);
		
		int64_t tid =(int64_t)list.getTermId16((char *)k);

		// print out for events
		if ( tid && 
		     tid != lattid  && 
		     tid != lontid  &&
		     tid != lattid2 && 
		     tid != lontid2 &&
		     tid != starttid &&
		     tid != endtid ) {
			char *ss = "";
			if ( isTagTerm ) ss = " tagterm";
			printf("k.n1=%016"XINT64" k.n0=%016"XINT64" "
			       "tid=%015"UINT64" "
			       //"date=%010"UINT32" "
			       "eidrng=%"INT32"-%"INT32" "
			       "score=%03"INT32" docId=%012"INT64"%s%s\n" , 
			       KEY1((char *)k,16),KEY0((char *)k),
			       tid,
			       //list.getCurrentDate(),
			       (int32_t)a,(int32_t)b,
			       (int32_t)list.getScore((char *)k),
			       list.getCurrentDocId() , ss, dd );
		}
		else if ( tid == starttid || tid == endtid ) {
			// this will uncomplement it
			uint32_t cd = (uint32_t)list.getCurrentDate();
			char *desc;
			if      ( tid == starttid ) desc = "startTime";
			else if ( tid == endtid   ) desc = "endTime";
			// convert to date str
			time_t ts = (time_t)cd;
			struct tm *timeStruct = localtime ( &ts );
			char ppp[100];
			strftime(ppp,100,"%b-%d-%Y-%H:%M:%S",timeStruct);
			// but use time if its not
			// otherwise a lat/lon/time key
			printf("k.n1=%016"XINT64" "
			       "k.n0=%016"XINT64" "
			       "tid=%015"UINT64"=%s "
			       "time=%s(%"UINT32") "
			       "eventId=%03"INT32" docId=%012"INT64"%s\n" , 
			       KEY1((char *)k,16),
			       KEY0((char *)k),
			       tid,
			       desc,
			       ppp,
			       cd,
			       (int32_t)list.getScore((char *)k),
			       list.getCurrentDocId() , 
			       dd );
		}
		else if ( tid ) {
			// this will uncomplement it
			uint32_t cd = list.getCurrentDate();
			// convert to float
			float latlon = (float)cd;
			// denormalize (we scaled by 10M)
			latlon /= 10000000.0;
			char *desc;
			if ( tid == lattid ) desc = "latitude";
			else if ( tid == lontid ) desc = "longitude";
			else if ( tid == lattid2 ) desc = "latitude2";
			else if ( tid == lontid2 ) desc = "longitude2";
			else desc = "unknownitude";
			// but use time if its not
			// otherwise a lat/lon/time key
			printf("k.n1=%016"XINT64" "
			       "k.n0=%016"XINT64" "
			       "tid=%015"UINT64" "
			       "%s=%.06f "
			       "eventId=%03"INT32" docId=%012"INT64"%s\n" , 
			       KEY1((char *)k,16),
			       KEY0((char *)k),
			       tid,
			       desc,
			       latlon,
			       (int32_t)list.getScore((char *)k),
			       list.getCurrentDocId() , 
			       dd );
		}

		/*
		if ( termId < 0 )
			printf("k.n1=%016"XINT64" k.n0=%016"XINT64" "
			       "tid=%015llu date=%010"UINT32" "
			       "score=%03"INT32" docId=%012"INT64"%s\n" , 
			       KEY1(k,16),KEY0(k),
			       (int64_t)list.getTermId16(k),
			       list.getCurrentDate(),
			       (int32_t)list.getScore(k),
			       list.getCurrentDocId() , dd );
		else
			printf("k.n1=%016"XINT64" k.n0=%016"XINT64" "
			       "date=%010"UINT32" score=%03"INT32" docId=%012"INT64"%s\n" , 
			       KEY1(k,16),KEY0(k),
			       list.getCurrentDate(),
			       (int32_t)list.getScore(k),
			       list.getCurrentDocId() , dd );
		*/
		continue;
	}

	KEYSET(startKey,list.getLastKey(),16);
	KEYADD(startKey,16);
	// watch out for wrap around
	//if ( startKey < *(key_t *)list.getLastKey() ) return;
	if ( KEYCMP(startKey,list.getLastKey(),16)<0 ) {
		g_threads.enableThreads();
		return;
	}
	goto loop;
}

void dumpClusterdb ( char *coll,
		     int32_t startFileNum,
		     int32_t numFiles,
		     bool includeTree ) {
	// this is confidential data format
#ifdef _CLIENT_
	return;
#endif
	g_clusterdb.init ();
	//g_collectiondb.init(true);
	g_clusterdb.getRdb()->addRdbBase1(coll );
	key_t startKey ;
	key_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;

	// bail if not
	if ( g_clusterdb.getRdb()->getNumFiles() <= startFileNum ) {
		printf("Request file #%"INT32" but there are only %"INT32" "
		       "clusterdb files\n",startFileNum,
		       g_clusterdb.getRdb()->getNumFiles());
		return;
	}

	Msg5 msg5;
	RdbList list;
	CollectionRec *cr = g_collectiondb.getRec(coll);
 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_CLUSTERDB ,
			      cr->m_collnum          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() )
		return;
	// loop over entries in list
	char strLanguage[256];
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k    = list.getCurrentKey();
		// is it a delete?
		char *dd = "";
		if ( (k.n0 & 0x01) == 0x00 ) dd = " (delete)";
		// get the language string
		languageToString ( g_clusterdb.getLanguage((char*)&k),
				   strLanguage );
		//uint32_t gid = getGroupId ( RDB_CLUSTERDB , &k );
		uint32_t shardNum = getShardNum( RDB_CLUSTERDB , &k );
		Host *grp = g_hostdb.getShard ( shardNum );
		Host *hh = &grp[0];
		// print it
		printf("k.n1=%08"XINT32" k.n0=%016"XINT64" "
		       "docId=%012"INT64" family=%"UINT32" "
		       "language=%"INT32" (%s) siteHash26=%"UINT32"%s " 
		       "groupNum=%"UINT32" "
		       "shardNum=%"UINT32"\n", 
		       k.n1, k.n0,
		       g_clusterdb.getDocId((char*)&k) , 
		       g_clusterdb.hasAdultContent((char*)&k) ,
		       (int32_t)g_clusterdb.getLanguage((char*)&k),
		       strLanguage,
		       g_clusterdb.getSiteHash26((char*)&k)    ,
		       dd ,
		       hh->m_hostId ,
		       shardNum);
		continue;
	}

	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key_t *)list.getLastKey() )
		return;
	goto loop;
}

/*
void dumpStatsdb( int32_t startFileNum, int32_t numFiles, bool includeTree,
		  int test ) {
		 
	// this is confidential data format
#ifdef _CLIENT_
	return;
#endif
	static char *coll = "stats";
	// We don't want to close the previous session so we
	// must not do a real init.
	g_statsdb.init( );//false - Is full init? 
	g_collectiondb.init( true ); // Is dump?
	g_statsdb.getRdb()->addRdbBase1 ( coll );

	uint64_t ss_keys = 0;
	uint64_t dd_keys = 0;
	key96_t startKey;
	key96_t endKey;
	startKey.setMin();
	endKey.setMax();

	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;

	// bail if not
	if ( g_statsdb.getRdb()->getNumFiles() <= startFileNum ) {
		printf("Request file #%"INT32" but there are only %"INT32" "
		       "statsdb files\n",startFileNum,
		       g_statsdb.getRdb()->getNumFiles());
		return;
	}

	Msg5 msg5;
	Msg5 msg5b;
	RdbList list;

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_STATSDB   ,
			      coll          ,
			      &list         ,
			      (char *)&startKey      ,
			      (char *)&endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() )
		return;
	// loop over entries in list
	key96_t k;
	time_t dateStamp;
	char txtDate[32];
	char *txt;

	uint64_t uCRC	 = 0LL;
	uint8_t  version = 0;
	int32_t dataSize = 0;

	SafeBuf cBuf( 1024 );
	bool dataSummaryGen = false;
	bool first = true;
	StatsV1 stats;

	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		
		list.getCurrentKey( (char *)&k );
		version = g_statsdb.getVersionFromKey( k );
		// is it a delete?
		char *dd = "";
		if (!( k.n0 & 0x01LL)) dd = " (delete)";
		dateStamp = g_statsdb.getTimestampFromKey( k );
		snprintf( txtDate, 32, "%s", ctime( &dateStamp ) );
		txt = txtDate;
                // get rid of the newline character
		while ( *txt ) {
			if ( *txt == '\n' ) { *txt = 0; break; } txt++;
		}

		// . We extract and verify the size of the data.
		// . If uCRC is zero, we failed to decompress the data.
		if ( k.n1 & SUMMARY_MASK ) {
			dataSummaryGen = true;
			cBuf.setBuf( list.getCurrentData(),
				     list.getCurrentDataSize(),
				     list.getCurrentDataSize(),
		false, //ownData
		csOther);//encoding
			if ( version == 1 ) {
				if ( ! stats.fromCompressed( cBuf ) ) {
                                        printf("Decompression Failed!!\n");
                                }
				dataSize = sizeof( StatsDataV1 );
				uCRC = g_statsdb.quickCRC(
						(uint8_t *)stats.getData(),
						dataSize );
			}
		}
		// print it
		if ( test != 3 && ! g_statsdb.getResolutionFromKey( k ) ) {
			printf("[Session Header Key] "
			       "k.n1=%08"XINT32" k.n0=%016"XINT64" resolution=%03"UINT32" "
			       "session=%05d timestamp=%010"INT32" [%s] "
			       "hostId=%05"UINT32" version=%03"UINT32" %s\n",
			       k.n1 , k.n0 ,
			       (uint32_t)g_statsdb.getResolutionFromKey(k),
			       (int16_t)g_statsdb.getSessionFromKey(k) ,
			       (int32_t)dateStamp ,
			       txtDate ,
			       (uint32_t)g_statsdb.getHostIdFromKey(k) ,
			       (uint32_t)version , 
			       dd );
			ss_keys++;
		} else if ( test == 2 ){
			printf("k.n1=0x%08"XINT32" k.n0=0x%016"XINT64" resolution=%03"UINT32" "
			       "session=%05d timestamp=%010"INT32" [%s] "
			       "hostId=%05"UINT32" version=%03"UINT32" "
			       "uLen=%010"UINT32" cLen=%010"UINT32" uCRC=%016"XINT64" %s \n",
			       k.n1 , k.n0 ,
			       (uint32_t)g_statsdb.getResolutionFromKey(k),
			       (int16_t)g_statsdb.getSessionFromKey(k) ,
			       (int32_t)dateStamp ,
			       txtDate ,
			       (uint32_t)g_statsdb.getHostIdFromKey( k ) ,
			       (uint32_t)version ,
			       (uint32_t)dataSize ,
			       list.getCurrentDataSize() ,
			       uCRC,
			       dd );
			dd_keys++;
		}
		else if ( test > 2 && first ) {
			StatsDataV1 &sData = *(StatsDataV1 *)stats.getData();
			printf("k.n1=0x%08"XINT32" k.n0=0x%016"XINT64" resolution=%03"UINT32" "
			       "session=%05d timestamp=%010"INT32" [%s] "
			       "hostId=%05"UINT32" version=%03"UINT32" "
			       "uLen=%010"UINT32" cLen=%010"UINT32" uCRC=%016"XINT64" %s \n"
			       "allQueries %"INT64"\n"
			       "msg3aRecallCnt %i\n"
			       "cpuUsage %f\n"
			       "",
			       k.n1 , k.n0 ,
			       (uint32_t)g_statsdb.getResolutionFromKey(k),
			       (int16_t)g_statsdb.getSessionFromKey(k) ,
			       (int32_t)dateStamp ,
			       txtDate ,
			       (uint32_t)g_statsdb.getHostIdFromKey( k ) ,
			       (uint32_t)version ,
			       (uint32_t)dataSize ,
			       list.getCurrentDataSize() ,
			       uCRC, dd,
			       sData.m_allQueries,
			       sData.m_msg3aRecallCnt,
			       sData.m_cpuUsage
			       );
			dd_keys++;
			if ( test == 3 ) {
				first = false;
                                printf( "\nPlease wait...\n\n" );
                        }
		}
	}
	
	startKey  = *(key96_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key96_t *)list.getLastKey() ) {
		printf( "Session Summary Keys: %"UINT64"\n"
			"Data Keys:            %"UINT64"\n",
			ss_keys, dd_keys );
		return;
	}
	goto loop;
}
*/

void dumpLinkdb ( char *coll,
		  int32_t startFileNum,
		  int32_t numFiles,
		  bool includeTree ,
		  char *url ) {
	// this is confidential data format
#ifdef _CLIENT_
	return;
#endif
	g_linkdb.init ();
	//g_collectiondb.init(true);
	g_linkdb.getRdb()->addRdbBase1(coll );
	key224_t startKey ;
	key224_t endKey   ;
	startKey.setMin();
	endKey.setMax();
	// set to docid
	if ( url ) {
		Url u;
		u.set ( url , gbstrlen(url) , true, false, false, false, false, 0x7fffffff ); // addWWW?
		uint32_t h32 = u.getHostHash32();//g_linkdb.getUrlHash(&u)
		int64_t uh64 = hash64n(url,0);
		startKey = g_linkdb.makeStartKey_uk ( h32 , uh64 );
		endKey   = g_linkdb.makeEndKey_uk   ( h32 , uh64 );
	}
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;

	// bail if not
	if ( g_linkdb.getRdb()->getNumFiles() <= startFileNum  && !includeTree) {
		printf("Request file #%"INT32" but there are only %"INT32" "
		       "linkdb files\n",startFileNum,
		       g_linkdb.getRdb()->getNumFiles());
		return;
	}

	Msg5 msg5;
	RdbList list;
	CollectionRec *cr = g_collectiondb.getRec(coll);

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_LINKDB ,
			      cr->m_collnum      ,
			      &list         ,
			      (char *)&startKey      ,
			      (char *)&endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         )){// err correction?
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key224_t k;
		list.getCurrentKey((char *) &k);
		// is it a delete?
		char *dd = "";
		if ( (k.n0 & 0x01) == 0x00 ) dd = " (delete)";
		int64_t docId = (int64_t)g_linkdb.getLinkerDocId_uk(&k);
		//if ( docId != 74785425291LL && docId != 88145066810LL )
		//	log("hey");
		//if ( list.m_listPtr-list.m_list >= 11784-24 )
		//	log("boo");
		//unsigned char hc = g_linkdb.getLinkerHopCount_uk(&k);
		//uint32_t gid = g_hostdb.getGroupId (RDB_LINKDB,&k,true);
		//int32_t groupNum = g_hostdb.getGroupNum ( gid );
		int32_t shardNum = getShardNum(RDB_LINKDB,&k);
		//if ( hc != 0 ) { char *xx=NULL;*xx=0; }
		// is it an ip or url record?
		//bool isHost = g_linkdb.isHostRecord ( &k );
		// is it a url or site key?
		//bool isUrlKey = g_linkdb.isUrlKey ( &k );
		// print this record type different
		//if ( isUrlKey ) {
		//int32_t ip = g_linkdb.getIp2(&k);
		//char *ipString = iptoa(ip);
		printf("k=%s "
		       "linkeesitehash32=0x%08"XINT32" "
		       "linkeeurlhash=0x%012"XINT64" "
		       "linkspam=%"INT32" "
		       "siterank=%02"INT32" "
		       //"hopcount=%03hhu "
		       "ip32=%s "
		       "docId=%012"UINT64" "
		       "discovered=%"UINT32" "
		       "lost=%"UINT32" "
		       "sitehash32=0x%08"XINT32" "
		       "shardNum=%"UINT32" "
		       "%s\n",
		       KEYSTR(&k,sizeof(key224_t)),
		       (int32_t)g_linkdb.getLinkeeSiteHash32_uk(&k),
		       (int64_t)g_linkdb.getLinkeeUrlHash64_uk(&k),
		       (int32_t)g_linkdb.isLinkSpam_uk(&k),
		       (int32_t)g_linkdb.getLinkerSiteRank_uk(&k),
		       //hc,//g_linkdb.getLinkerHopCount_uk(&k),
		       iptoa((int32_t)g_linkdb.getLinkerIp_uk(&k)),
		       docId,
		       (int32_t)g_linkdb.getDiscoveryDate_uk(&k),
		       (int32_t)g_linkdb.getLostDate_uk(&k),
		       (int32_t)g_linkdb.getLinkerSiteHash32_uk(&k),
		       shardNum,
		       dd );
	}

	startKey = *(key224_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key224_t *)list.getLastKey() ) return;
	goto loop;
}


bool pingTest ( int32_t hid , uint16_t clientPort ) {
	Host *h = g_hostdb.getHost ( hid );
	if ( ! h ) return log("net: pingtest: hostId %"INT32" is "
			      "invalid.",hid);
        // set up our socket
        int sock  = socket ( AF_INET, SOCK_DGRAM , 0 );
        if ( sock < 0 ) return log("net: pingtest: socket: %s.",
				   strerror(errno));

        // sockaddr_in provides interface to sockaddr
        struct sockaddr_in name; 
        // reset it all just to be safe
        memset((char *)&name, 0,sizeof(name));
        name.sin_family      = AF_INET;
        name.sin_addr.s_addr = INADDR_ANY;
        name.sin_port        = htons(clientPort);
        // we want to re-use port it if we need to restart
        int options = 1;
        if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR ,
			&options,sizeof(options)) < 0 ) 
		return log("net: pingtest: setsockopt: %s.", 
			   strerror(errno));
        // bind this name to the socket
        if ( bind ( sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
               close ( sock );
               return log("net: pingtest: Bind on port %hu: %s.",
			  clientPort,strerror(errno));
	}

	//g_loop.setNonBlocking ( sock , 0 );
	//g_loop.interruptsOff();
	int fd = sock;
	int flags = fcntl ( fd , F_GETFL ) ;
	if ( flags < 0 )
		return log("net: pingtest: fcntl(F_GETFL): %s.",
			   strerror(errno));
	//if ( fcntl ( fd, F_SETFL, flags|O_NONBLOCK|O_ASYNC) < 0 )
	//return log("db: Loop::addSlot:fcntl(NONBLOCK):%s",strerror(errno));

	char dgram[1450];
	int n;
	struct sockaddr_in to;
	sockaddr_in from;
	socklen_t fromLen;
	int64_t startTime;

	// make the dgram
	UdpProtocol *up = &g_dp; // udpServer2.getProtocol();
	int32_t transId = 500000000 - 1 ;
	int32_t dnum    = 0; // dgramNum

	int32_t sends     = 0;
	int32_t lost      = 0;
	int32_t recovered = 0;
	int32_t acks      = 0;
	int32_t replies   = 0;

	int32_t ip = h->m_ip;
	ip = atoip("127.0.0.1",9);

	startTime = gettimeofdayInMilliseconds_force();
	to.sin_family      = AF_INET;
	to.sin_addr.s_addr = h->m_ip;
	to.sin_port        = ntohs(h->m_port);
	memset ( &(to.sin_zero) , 0,8 );
	log("net: pingtest: Testing hostId #%"INT32" at %s:%hu from client "
	    "port %hu", hid,iptoa(h->m_ip),h->m_port,clientPort);
	// if this is higher than number of avail slots UdpServer.cpp
	// will not be able to free the slots and this will end up sticking,
	// because the slots can only be freed in destroySlot() which
	// is not async safe!
	//int32_t count = 40000; // number of loops
	int32_t count = 1000; // number of loops
	int32_t avg = 0;
 sendLoop:
	if ( count-- <= 0 ) {
		log("net: pingtest: Got %"INT32" replies out of %"INT32" sent (%"INT32" lost)"
		    "(%"INT32" recovered)", replies,sends,lost,recovered);
		log("net: pingtest: Average reply time of %.03f ms.",
		    (double)avg/(double)replies);
		return true;
	}
	transId++;
	int32_t msgSize = 3; // indicates a debug ping packet to PingServer.cpp
	up->setHeader ( dgram, msgSize, 0x11, dnum, transId, true, false , 0 );
	int32_t size = up->getHeaderSize(0) + msgSize;
	int64_t start = gettimeofdayInMilliseconds_force();
	// debug
	//log("db: sending %"INT32" bytes",size);
	n = sendto(sock,dgram,size,0,(struct sockaddr *)&to,sizeof(to));
	if ( n != size ) return log("net: pingtest: sendto returned "
				    "%i "
				    "(should have returned %"INT32")",n,size);
	sends++;
 readLoop2:
	// loop until we read something
	n = recvfrom (sock,dgram,DGRAM_SIZE,0,(sockaddr *)&from, &fromLen);
	if (gettimeofdayInMilliseconds_force() - start>2000) {lost++; goto sendLoop;}
	if ( n <= 0 ) goto readLoop2; // { sched_yield(); goto readLoop2; }
	// for what transId?
	int32_t tid = up->getTransId ( dgram , n );
	// -1 is error
	if ( tid < 0 ) return log("net: pingtest: Bad transId.");
	// if no match, it was recovered, keep reading
	if ( tid != transId ) { 
		log("net: pingTest: Recovered tid=%"INT32", current tid=%"INT32". "
		    "Resend?",tid,transId); 
		recovered++; 
		goto readLoop2; 
	}
	// an ack?
	if ( up->isAck ( dgram , n ) ) { 
		acks++; 
		// debug
		//log("db: read ack of %"INT32" bytes",n);
		goto readLoop2; 
	}
	// debug
	//log("db: read %"INT32" bytes",n);
	// mark the time
	int64_t took = gettimeofdayInMilliseconds_force()-start;
	if ( took > 1 ) log("net: pingtest: got reply #%"INT32" (tid=%"INT32") "
			    "in %"INT64" ms",replies,transId,took);
	// make average
	avg += took;
	// the reply?
	replies++;
	// send back an ack
	size = up->makeAck ( dgram, dnum, transId , true/*weinit?*/ , false );
	n = sendto(sock,dgram,size,0,(struct sockaddr *)&to,sizeof(to));
	// debug
	//log("db: send %"INT32" bytes",n);
	// mark our first read
	goto sendLoop;
}

int injectFileTest ( int32_t reqLen , int32_t hid ) {

	// make a mime
	char *req = (char *)mmalloc ( reqLen , "injecttest");
	if ( ! req ) return log("build: injecttest: malloc(%"INT32") "
				"failed", reqLen)-1;
	char *p    = req;
	char *pend = req + reqLen;
	sprintf ( p , 
		  "POST /inject HTTP/1.0\r\n"
		  "Content-Length: 000000000\r\n" // placeholder
		  "Content-Type: text/html\r\n"
		  "Connection: Close\r\n"
		  "\r\n" );
	p += gbstrlen(p);
	char *content = p;
	sprintf ( p , 
		  "u=%"UINT32".injecttest.com&c=&"
		  "deleteurl=0&ip=4.5.6.7&iplookups=0&"
		  "dedup=1&rs=7&"
		  "quick=1&hasmime=1&ucontent="
		  "HTTP 200\r\n"
		  "Last-Modified: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
		  "Connection: Close\r\n"
		  "Content-Type: text/html\r\n"
		  "\r\n" , 
		  (uint32_t)time(NULL) );
	p += gbstrlen(p);
	// now store random words (just numbers of 8 digits each)
	while ( p + 12 < pend ) {
		int32_t r ; r = rand(); 
		sprintf ( p , "%010"UINT32" " , r );
		p += gbstrlen ( p );
	}
	// set content length
	int32_t clen = p - content;
	char *ptr = req ;
	// find start of the 9 zeroes
	while ( *ptr != '0' || ptr[1] !='0' ) ptr++;
	// store length there
	sprintf ( ptr , "%09"UINT32"" , clen );
	// remove the \0
	ptr += gbstrlen(ptr); *ptr = '\r';

	// what is total request length?
	int32_t rlen = p - req;

	// generate the filename
	char *filename = "/tmp/inject-test";
	File f; 
	f.set ( filename );
	f.unlink();
	if ( ! f.open ( O_RDWR | O_CREAT ) )
		return log("build: injecttest: Failed to create file "
			   "%s for testing", filename) - 1;

	if ( rlen != f.write ( req , rlen , 0 ) ) 
		return log("build: injecttest: Failed to write %"INT32" "
			   "bytes to %s", rlen,filename) - 1;
	f.close();

	mfree ( req , reqLen , "injecttest" );

	Host *h = g_hostdb.getHost(hid);

	char *ips = iptoa(h->m_ip);

	// now inject the file
	//return injectFile ( filename , ips , 0 , MAX_DOCID , false );
	return injectFile ( filename , ips , "main");
}

#define MAX_INJECT_SOCKETS 300
//#define MAX_INJECT_SOCKETS 1
static void doInject ( int fd , void *state ) ;
static void doInjectWarc ( int64_t fsize );
static void doInjectArc ( int64_t fsize );
static void injectedWrapper ( void *state , TcpSocket *s ) ;
static TcpServer s_tcp;
static File      s_file;
static int64_t s_off = 0; // offset into file
static int32_t      s_ip;
static int16_t     s_port;
static Hostdb s_hosts2;
static int32_t s_rrn = 0;
static int32_t      s_registered = 1;
static int32_t      s_maxSockets = MAX_INJECT_SOCKETS;
static int32_t      s_outstanding = 0;
static bool s_isDelete;
static int32_t s_injectTitledb;
static int32_t s_injectWarc;
static int32_t s_injectArc;
static char *s_coll = NULL;
static key_t s_titledbKey;
static char *s_req  [MAX_INJECT_SOCKETS];
static int64_t s_docId[MAX_INJECT_SOCKETS];
static char s_init5 = false;
static int64_t s_endDocId;

int injectFile ( char *filename , char *ips , 
		 //int64_t startDocId ,
		 //int64_t endDocId ,
		 //bool isDelete ) {
		 char *coll ) {



	// or part of an itemlist.txt-N
	int flen2 = gbstrlen(filename);
	if ( flen2>=14 && strncmp(filename,"itemlist.txt",12)==0 ) {
	        // must have -N
		int split = atoi(filename+13);
		log("inject: using part file of itemlist.txt of %i",split);
		// open it
		SafeBuf sb;
		sb.load("./itemlist.txt");
		// scan the lines
		char *p = sb.getBufStart();
		char *pend = p + sb.length();
		int count = 0;
		char *nextLine = NULL;
		for (  ; p && p < pend ; p = nextLine ) {
			nextLine = strstr(p,"\n");
			if ( nextLine ) nextLine++;
			// this is how many hosts we are using!!
			// TODO: make this get from hosts.conf!!!
			if ( count >= 40 ) count = 0;
			if ( count++ != split ) continue;
			// get line
			char *archiveDirName = p;
			if ( nextLine ) nextLine[-1] = '\0';
			// download the archive
			SafeBuf cmd;
			cmd.safePrintf("./ia download "
				       //"--format=\"Web ARChive GZ\" "
				       "--glob='*arc.gz' "
				       "%s"
				       ,archiveDirName);
			gbsystem(cmd.getBufStart());
			// now inject the warc gz files in there
			Dir dir;
			dir.set ( p );
			dir.open();
			log("setting dir to %s",p);
		subloop:
			char *xarcFilename = dir.getNextFilename("*arc.gz");
			// get next archive
			if ( ! xarcFilename ) {
				cmd.reset();
				// remove the archive dir when done if
				// no more warc.gz files in it
				cmd.safePrintf("rm -rf %s",archiveDirName);
				gbsystem(cmd.getBufStart());
				// download the next archive using 'ia'
				continue;
			}
			int32_t flen = gbstrlen(xarcFilename);
			char *ext = xarcFilename + flen -7;
			// gunzip to foo.warc or foo.arc depending!
			char *es = "";
			if ( ext[0] == 'w' ) es = "w";
			// inject the warc.gz files
			cmd.reset();
			cmd.safePrintf("gunzip -c %s/%s > ./foo%i.%sarc"
				       ,archiveDirName,xarcFilename,split,es);
			gbsystem(cmd.getBufStart());
			// now inject it
			cmd.reset();
			cmd.safePrintf("./gbi inject ./foo%i.%sarc hosts.conf"
				       ,split,es);
			gbsystem(cmd.getBufStart());
			goto subloop;
		}
		log("cmd: done injecting archives for split %i",split);
		exit(0);
	}

	bool isDelete = false;
	int64_t startDocId = 0LL;
	int64_t endDocId = MAX_DOCID;

	g_conf.m_maxMem = 4000000000LL;
	g_mem.init ( );//4000000000LL );

	// set up the loop
	if ( ! g_loop.init() ) return log("build: inject: Loop init "
					  "failed.")-1;
	// init the tcp server, client side only
	if ( ! s_tcp.init( NULL , // requestHandlerWrapper       ,
			   getMsgSize, 
			   NULL , // getMsgPiece                 ,
			   0    , // port, only needed for server ,
			   &s_maxSockets    ) ) return false;

	s_tcp.m_doReadRateTimeouts = false;

	s_isDelete = isDelete;

	if ( ! s_init5 ) {
		s_init5 = true;
		for ( int32_t i = 0; i < MAX_INJECT_SOCKETS ; i++ )
			s_req[i] = NULL;
	}

	// get host
	//Host *h = g_hostdb.getHost ( hid );
	//if ( ! h ) return log("build: inject: Hostid %"INT32" is invalid.",
	//		      hid)-1;
	char *colon = strstr(ips,":");
	int32_t port = 8000;
	if ( colon ) {
		*colon = '\0';
		port = atoi(colon+1);
	}
	int32_t ip = 0;
	// is ip field a hosts.conf instead? that means to round robin.
	if ( strstr(ips,".conf") ) {
		if ( ! s_hosts2.init ( -1 ) ) { // ips , 0 ) ) {
			fprintf(stderr,"failed to load %s",ips);
			exit(0);
		}
		s_ip = 0;
		s_port = 0;
	}
	else {
		ip = atoip(ips,strlen(ips));
		if ( ip == 0 || ip == -1 ) {
			log("provided ip \"%s\" is a bad ip. "
				"exiting\n",ips);
			exit(0);
		}
		if ( port == 0 || port == -1 ) {
			log("bad port. exiting\n");
			exit(0);
		}
		s_ip   = ip;//h->m_ip;
		s_port = port;//h->m_httpPort;
	}

	s_injectTitledb = false;

	//char *coll = "main";
	if ( strncmp(filename,"titledb",7) == 0 ) {
		//int32_t hostId = 0;
		//Host *h = g_hostdb.getHost ( hostId );
		//if ( ! h ) { log("db: No host has id %"INT32".",hostId); exit(0);}
		//if ( ! g_conf.init ( h->m_dir ) ) { // , h->m_hostId ) ) {
		//	log("db: Conf init failed." ); exit(0); }
		// a new thing, titledb-gk144 or titledb-coll.main.0
		// init the loop, needs g_conf
		if ( ! g_loop.init() ) {
			log("db: Loop init failed." ); exit(0); }
		// set up the threads, might need g_conf
		if ( ! g_threads.init() ) {
			log("db: Threads init failed." ); exit(0); }
		s_injectTitledb = true;
		s_titledbKey.setMin();

		// read where we left off from file if possible
		char fname[256];
		//sprintf(fname,"%s/lastinjectdocid.dat",g_hostdb.m_dir);
		sprintf(fname,"./lastinjectdocid.dat");
		SafeBuf ff;
		ff.fillFromFile(fname);
		if ( ff.length() > 1 ) {
			int64_t ffdocId = atoll(ff.getBufStart() );
			// if process got killed in the middle of write
			// i guess the stored docid could be corrupted!
			// so make sure its in startDocId,endDocId range
			if ( ffdocId > 0 && 
			     ffdocId >= startDocId &&
			     ffdocId < endDocId )
				startDocId = ffdocId;
			else
				log("build: saved docid %"INT64" not "
				    "in [%"INT64",%"INT64"]",
				    ffdocId,
				    startDocId,
				    endDocId );
		}

		if ( startDocId != 0LL )
			s_titledbKey = g_titledb.makeFirstKey(startDocId);

		s_endDocId = endDocId;

		// so we do not try to merge files, or write any data:
		g_dumpMode = true;
		//g_conf.m_spiderdbMaxDiskPageCacheMem   = 0;
		//g_conf.m_urldbMaxDiskPageCacheMem = 0;

		// . add a fake coll just for it.
		// . make the subdir just gk144 not coll.gk144.0 so rick
		//   can inject the titledb bigfile
		//g_collectiondb.init(true);
		/*
		g_collectiondb.addRec ( "poo" , // char *coll , 
					NULL , // char *cpc , 
					0 , // int32_t cpclen , 
					false , // bool isNew ,
					-1 , // collnum_t collnum , 
					false , // bool isDump ,
					false ); // bool saveIt
		*/
		CollectionRec *cr = new (CollectionRec);
		SafeBuf *rb = &g_collectiondb.m_recPtrBuf;
		rb->reserve(4);
		g_collectiondb.m_recs = (CollectionRec **)rb->getBufStart();
		g_collectiondb.m_recs[0] = cr;

		// right now this is just for the main collection
		char *coll = "main";
		addCollToTable ( coll , (collnum_t) 0 );

		// force RdbTree.cpp not to bitch about corruption
		// assume we are only getting out collnum 0 recs i guess
		g_collectiondb.m_numRecs = 1;
		g_titledb.init ();
		//g_titledb.getRdb()->addRdbBase1(coll );
		// msg5::readList() requires the RdbBase for collnum 0
		// which holds the array of files and the tree
		Rdb *rdb = g_titledb.getRdb();
		static RdbBase *s_base = new ( RdbBase );
		// so getRdbBase always returns 
		rdb->m_collectionlessBase = s_base;
		rdb->m_isCollectionLess = true;
		//CollectionRec *pcr = g_collectiondb.getRec((collnum_t)0);
		//pcr->m_bases[RDB_TITLEDB] = s_base;
		// dir for tree loading
		sprintf(g_hostdb.m_dir , "./" );
		rdb->loadTree();
		// titledb-
		if ( gbstrlen(filename)<=8 )
			return log("build: need titledb-coll.main.0 or "
			    "titledb-gk144 not just 'titledb'");
		char *coll2 = filename + 8;

		char tmp[1024];
		sprintf(tmp,"./%s",coll2);
		s_base->m_dir.set(tmp);
		strcpy(s_base->m_dbname,rdb->m_dbname);
		s_base->m_dbnameLen = gbstrlen(rdb->m_dbname);
		s_base->m_coll = "main";
		s_base->m_collnum = (collnum_t)0;
		s_base->m_rdb = rdb;
		s_base->m_fixedDataSize = rdb->m_fixedDataSize;
		s_base->m_useHalfKeys = rdb->m_useHalfKeys;
		s_base->m_ks = rdb->m_ks;
		s_base->m_pageSize = rdb->m_pageSize;
		s_base->m_isTitledb = rdb->m_isTitledb;
		s_base->m_minToMerge = 99999;
		// try to set the file info now!
		s_base->setFiles();
	}
	else {
		// open file
		s_file.set ( filename );
		if ( ! s_file.open ( O_RDONLY ) )
			return log("build: inject: Failed to open file %s "
				   "for reading.", filename) - 1;
		s_off = 0;
	}

	// this might be a compressed warc like .warc.gz
	s_injectWarc = false;
	s_injectArc  = false;
	int flen = gbstrlen(filename);
	if ( flen>5 && strcasecmp(filename+flen-5,".warc")==0 ) {
		s_injectWarc = true;
	}
	if ( flen>5 && strcasecmp(filename+flen-4,".arc")==0 ) {
		s_injectArc = true;
	}

	
	s_coll = coll;

	if ( ! s_coll ) s_coll = "main";

	// register sleep callback to get started
	if ( ! g_loop.registerSleepCallback(1, NULL, doInject) )
		return log("build: inject: Loop init failed.")-1;
	// run the loop
	if ( ! g_loop.runLoop() ) return log("build: inject: Loop "
					     "run failed.")-1;
	// dummy return
	return 0;
}

void doInject ( int fd , void *state ) {

	if ( s_registered ) {
		s_registered = 0;
		g_loop.unregisterSleepCallback ( NULL, doInject );
	}
	
	// turn off threads so this happens right away
	g_conf.m_useThreads = false;

	int64_t fsize ;
	if ( ! s_injectTitledb ) fsize = s_file.getFileSize();

	// just repeat the function separately. i guess we'd repeat
	// some code but for simplicity i think it is worth it. and we
	// should probably phase out the ++++URL: format thing.
	if ( s_injectWarc ) {
		doInjectWarc ( fsize );
		return;
	}

	if ( s_injectArc ) {
		doInjectArc ( fsize );
		return;
	}



 loop:

	int32_t reqLen;
	int32_t reqAlloc;
	char *req;

	// if reading from our titledb and injecting into another cluster
	if ( s_injectTitledb ) {
		// turn off threads so this happens right away
		g_conf.m_useThreads = false;
		key_t endKey; //endKey.setMax();
		endKey = g_titledb.makeFirstKey(s_endDocId);
		RdbList list;
		Msg5 msg5;
		Msg5 msg5b;
		char *coll = "main";
		CollectionRec *cr = g_collectiondb.getRec(coll);
		msg5.getList ( RDB_TITLEDB ,
			       cr->m_collnum,
			       &list         ,
			       (char *)&s_titledbKey ,
			       (char *)&endKey        ,
			       100 , // minRecSizes   ,
			       true , // includeTree   ,
			       false         , // add to cache?
			       0             , // max cache age
			       0 , // startFileNum  ,
			       -1, // numFiles      ,
			       NULL          , // state
			       NULL          , // callback
			       0             , // niceness
			       false         , // err correction?
			       NULL           , // cache key ptr
			       0              , // retry num
			       -1             , // maxRetries
			       true           , // compensate for merge
			       -1LL           , // sync point
			       &msg5b         );
		// all done if empty
		if ( list.isEmpty() ) { g_loop.reset();  exit(0); }
		// loop over entries in list
		list.getCurrentKey((char *) &s_titledbKey);
		// advance for next
		s_titledbKey += 1;
		// is it a delete?
		char *rec     = list.getCurrentRec    ();
		int32_t  recSize = list.getCurrentRecSize();
		// skip negative keys!
		if ( (rec[0] & 0x01) == 0x00 ) goto loop;
		// re-enable threads i guess
		g_conf.m_useThreads = true;
		// set and uncompress
		//TitleRec tr;
		XmlDoc xd;
		if ( ! xd.set2 ( rec , 
				 recSize , 
				 coll ,
				 NULL , // safebuf
				 0 , // niceness
				 NULL ) ) { // spiderrequest
			log("build: inject skipping corrupt title rec" );
			goto loop;
		}
		// sanity!
		if ( xd.size_utf8Content > 5000000 ) {
			log("build: inject skipping huge title rec" );
			goto loop;
		}
		// get the content length. uenc can be 2140 bytes! seen it!
		reqAlloc = xd.size_utf8Content + 6000;
		// make space for content
		req = (char *)mmalloc ( reqAlloc , "maininject" );
		if ( ! req ) {
			log("build: inject: Could not allocate %"INT32" bytes for "
			    "request at offset %"INT64"",reqAlloc,s_off);
			exit(0);
		}
		char *ipStr = iptoa(xd.m_ip);
		// encode the url
		char *url = xd.getFirstUrl()->getUrl();
		char uenc[5000];
		urlEncode ( uenc , 4000 , url , strlen(url) , true );
		char *content = xd.ptr_utf8Content;
		int32_t  contentLen = xd.size_utf8Content;
		if ( contentLen > 0 ) contentLen--;
		char c = content[contentLen];
		content[contentLen] = '\0';
		//log("inject: %s",xd.m_firstUrl.m_url);
		// form what we would read from disk
		reqLen = sprintf(req,
				 // print as unencoded content for speed
				 "POST /inject HTTP/1.0\r\n"
				 "Content-Length: 000000000\r\n"//placeholder
				 "Content-Type: text/html\r\n"
				 "Connection: Close\r\n"
				 "\r\n"
				 // now the post cgi parms
				 "c=%s&"
				 // quick docid only reply
				 "quick=1&" 
				 // url of injecting page
				 "u=%s&" 
				 "ip=%s&"
				 //"firstip=%s&"
				 "firstindexed=%"UINT32"&"
				 "lastspidered=%"UINT32"&"
				 // prevent looking up firstips
				 // on all outlinks for speed:
				 "spiderlinks=0&"
				 "hopcount=%"INT32"&"
				 "newonly=2&"  // only inject if new
				 "dontlog=1&"
				 "charset=%"INT32"&"
				 "ucontent="
				 // first the mime
				 //"HTTP 200\r\n"
				 //"Connection: Close\r\n"
				 //"Content-Type: text/html\r\n"
				 //"Content-Length: %"INT32"\r\n"
				 //"\r\n"
				 // then the content of the injecting page
				 "%s"
				 , coll
				 , uenc
				 , ipStr
				 //, ipStr
				 , xd.m_firstIndexedDate
				 , xd.m_spideredTime
				 , (int32_t)*xd.getHopCount()
				 , (int32_t)xd.m_charset
				 //, contentLen
				 , content
				 );
		content[contentLen] = c;
		if ( reqLen >= reqAlloc ) { 
			log("inject: bad engineer here");
			char *xx=NULL;*xx=0; 
		}
		// set content length
		char *start = strstr(req,"c=");
		int32_t realContentLen = strlen(start);
		char *ptr = req ;
		// find start of the 9 zeroes
		while ( *ptr != '0' || ptr[1] !='0' ) ptr++;
		// store length there
		sprintf ( ptr , "%09"UINT32"" , realContentLen );
		// remove the \0
		ptr += strlen(ptr); *ptr = '\r';
		// map it
		int32_t i; for ( i = 0 ; i < MAX_INJECT_SOCKETS ; i++ ) {
			// skip if occupied
			if ( s_req[i] ) continue;
			s_req  [i] = req;
			s_docId[i] = xd.m_docId;
			break;
		}
		if ( i >= MAX_INJECT_SOCKETS )
			log("build: could not add req to map");
	}
	else {
		// are we done?
		if ( s_off >= fsize ) { 
			log("inject: done parsing file");
			g_loop.reset();  
			exit(0); 
		}
		// read the mime
		char buf [ 1000*1024 ];
		int32_t maxToRead = 1000*1024;
		int32_t toRead = maxToRead;
		if ( s_off + toRead > fsize ) toRead = fsize - s_off;
		int32_t bytesRead = s_file.read ( buf , toRead , s_off ) ;
		if ( bytesRead != toRead ) {
			log("build: inject: Read of %s failed at offset "
			    "%"INT64"", s_file.getFilename(), s_off);
			exit(0);
		}

		char *fend = buf + toRead;

		char *pbuf = buf;
		// partap padding?
		if ( pbuf[0] == '\n' ) pbuf++;
		if ( pbuf[0] == '\n' ) pbuf++;
		// need "++URL: "
		for ( ; *pbuf && strncmp(pbuf,"+++URL: ",8) ; pbuf++ );
		// none?
		if ( ! *pbuf ) {
			log("inject: done!");
			exit(0);
		}
		// sometimes line starts with "URL: http://www.xxx.com/\n"
		char *url = pbuf + 8; // NULL;
		// skip over url
		pbuf = strchr(pbuf,'\n');
		// null term url
		*pbuf = '\0';
		// log it
		log("inject: injecting url %s",url);
		// debug
		//if ( strstr(url,"worldexecutive.com") )
		//	log("poo");
		// skip to next line
		pbuf++;
		// get offset into "buf"
		int32_t len = pbuf - buf;
		// subtract that from toRead so it is the available bytes left
		toRead -= len;
		// advance this for next read
		s_off += len;

		//if ( ! strncmp(pbuf,"URL: ", 5 ) ) {
		// if it's not a mime header assume just a url
		//if ( strncmp(pbuf,"GET /",5) &&
		//     strncmp(pbuf,"POST /",6) ) {
		// skip "URL: "
		/*
		if ( strncmp(pbuf,"+++URL: ",8) == 0 )
				url = pbuf + 8;
			else
				url = pbuf;
			// find \n
			pbuf = strchr(pbuf,'\n');
			*pbuf = '\0';
			pbuf++;
			int32_t len = pbuf - buf;
			toRead -= len;
			s_off += len;
		}
		*/
		// should be a mime that starts with GET or POST
		//char *mimePtr = pbuf;
		HttpMime m;
		if ( ! m.set ( pbuf , toRead , NULL ) ) {
			if ( toRead > 128 ) toRead = 128;
			pbuf [ toRead ] = '\0';
			log("build: inject: Failed to set mime at offset "
			    "%"INT64" where request=%s",s_off,buf);
			exit(0);
		}
		// find the end of it, the next "URL: " line or
		// end of file
		char *p = pbuf;
		char *contentPtrEnd = fend;
		for ( ; p < fend ; p++ ) {
			if ( p[0] == '+' &&
			     p[1] == '+' &&
			     p[2] == '+' &&
			     p[3] == 'U' &&
			     p[4] == 'R' &&
			     p[5] == 'L' &&
			     p[6] == ':' &&
			     p[7] == ' ' ) {
				contentPtrEnd = p;
				break;
			}
		}
		// point to the content (NOW INCLUDE MIME!)
		char *contentPtr = pbuf;//  + m.getMimeLen();
		int32_t  contentPtrLen = contentPtrEnd - contentPtr;
		if ( contentPtrEnd == fend && bytesRead == maxToRead ) {
			log("inject: not reading enough content to inject "
			    "url %s . increase maxToRead from %"INT32"",url,
			    maxToRead);
			exit(0);
		}
		// get the length of content (includes the submime for 
		// injection)
		int32_t contentLen = m.getContentLen();
		if ( ! url && contentLen == -1 ) {
			log("build: inject: Mime at offset %"INT64" does not "
			    "specify required Content-Length: XXX field.",
			    s_off);
			exit(0);
		}
		// alloc space for mime and content
		//reqAlloc = 5000;
		//if ( ! url ) reqAlloc += m.getMimeLen() + contentLen ;
		reqAlloc = contentPtrLen + 2 + 6000;
		// make space for content
		req = (char *)mmalloc ( reqAlloc , "maininject" );
		if ( ! req ) {
			log("build: inject: Could not allocate %"INT32" bytes for "
			    "request at offset %"INT64"",reqAlloc,s_off);
			exit(0);
		}
		char *rp = req;
		// a different format?
		//if ( url ) {
		char *ipStr = "1.2.3.4";
		//int32_t recycle = 0;
		//if ( s_isDelete ) recycle = 1;
		rp += sprintf(rp,
			      "POST /inject HTTP/1.0\r\n"
			      "Content-Length: 000000000\r\n"//bookmrk
			      "Content-Type: text/html\r\n"
			      "Connection: Close\r\n"
			      "\r\n"
			      "c=main&"
			      // do parsing consistency testing (slower!)
			      //"dct=1&"
			      // mime is in the "&ucontent=" parm
			      "hasmime=1&"
			      // prevent looking up firstips
			      // on all outlinks for speed:
			      "spiderlinks=0&"
			      "quick=1&" // quick reply
			      "dontlog=1&"
			      "ip=%s&"
			      //"recycle=%"INT32"&"
			      "deleteurl=%"INT32"&"
			      "u=",
			      ipStr,
			      //recycle,
			      (int32_t)s_isDelete);
		// url encode the url
		rp += urlEncode ( rp , 4000 , url , gbstrlen(url) );
		// finish it up
		rp += sprintf(rp,"&ucontent=");
		//}

		if ( ! url ) {
			// what is this?
			char *xx=NULL;*xx=0;
			/*
			// stick mime in there
			gbmemcpy ( rp , mimePtr , m.getMimeLen() );
			// skip that
			rp += m.getMimeLen();
			// turn \n\n into \r\n\r\n
			if ( rp[-2] == '\n' && rp[-1] == '\n' ) {
				rp[-2] = '\r';
				rp[ 0] = '\r';
				rp[ 1] = '\n';
				rp += 2;
			}
			// advance
			s_off += m.getMimeLen();
			// read from file into content
			int32_t contRead = contentLen;
			if ( s_off + contRead > fsize ) {
				log("build: inject: Content-Length of %"INT32" "
				    "specified "
				    "for content at offset %"INT64" would breech "
				    "EOF",
				    contentLen,s_off);
				exit(0);
			}
			if ( contRead != s_file.read ( rp ,contRead , s_off)) {
				log("build: inject: Read of %s failed at "
				    "offset %"INT64"",
				    s_file.getFilename(), s_off);
				exit(0);
			}
			// skip that
			rp += contRead;
			// success
			s_off += contRead;
			*/
		}

		// store the content after the &ucontent
		gbmemcpy ( rp , contentPtr , contentPtrLen );
		rp += contentPtrLen;

		s_off += contentPtrLen;

		// just for ease of display
		*rp = '\0';


		// set content length
		char *start = strstr(req,"c=");
		int32_t realContentLen = gbstrlen(start);
		char *ptr = req ;
		// find start of the 9 zeroes
		while ( *ptr != '0' || ptr[1] !='0' ) ptr++;
		// store length there
		sprintf ( ptr , "%09"UINT32"" , realContentLen );
		// remove the \0
		ptr += strlen(ptr); *ptr = '\r';

		// set this
		reqLen = rp - req;
		// sanity
		if ( reqLen > reqAlloc ) { char *xx=NULL;*xx=0; }
	}

	int32_t ip = s_ip;
	int32_t port = s_port;

	// try hosts.conf
	if ( ip == 0 ) {
		// round robin over hosts in s_hosts2
		if ( s_rrn >= s_hosts2.getNumHosts() ) s_rrn = 0;
		Host *h = s_hosts2.getHost ( s_rrn );
		ip = h->m_ip;
		port = h->m_httpPort;
		s_rrn++;
	}

	// now inject it
	bool status = s_tcp.sendMsg( NULL, 0, ip, port, req, reqAlloc, reqLen, reqLen, NULL, injectedWrapper,
								 9999 * 60 * 1000, -1, -1 );

	// launch another if blocked
	//if ( ! status ) return;
	if ( ! status ) {
		//int32_t nh = g_hostdb.getNumHosts();
		//nh = (nh * 15) / 10;
		//if ( nh > MAX_INJECT_SOCKETS - 10 ) 
		//	nh = MAX_INJECT_SOCKETS - 10;
		//if ( nh < 5 ) nh = 5;
		// limit to one socket right now
		//if ( ++s_outstanding < 1 ) goto loop;
		if ( ++s_outstanding < MAX_INJECT_SOCKETS ) goto loop;
		return;
	}
		
	if ( g_errno ) 
		log("build: inject had error: %s.",mstrerror(g_errno));
	// free if did not block, tcpserver frees on immediate error
	else
		mfree ( req , reqAlloc , "maininject" );
	// loop if not
	goto loop;
}


// 100MB per warc rec max
#define MAXWARCRECSIZE 100*1024*1024

void doInjectWarc ( int64_t fsize ) {

	static char *s_buf = NULL;

	static bool s_hasMoreToRead;

	static char *s_pbuf = NULL;
	static char *s_pbufEnd = NULL;

	bool needReadMore = false;
	if ( ! s_pbuf ) needReadMore = true;


 readMore:

	if ( needReadMore ) {

		log("inject: reading %"INT64" bytes more of warc file"
		    ,(int64_t)MAXWARCRECSIZE);

		// are we done?
		if ( s_off >= fsize ) { 
			log("inject: done parsing warc file");
			if ( s_outstanding ) {
				log("inject: waiting for socks");return;}
			g_loop.reset();  
			exit(0); 
		}

		// read 1MB of data into this buf to get the first WARC record
		// it must be < 1MB or we faulter.
		if ( ! s_buf ) {
			int64_t need = MAXWARCRECSIZE + 1;
			s_buf = (char *)mmalloc ( need ,"sibuf");
		}
		if ( ! s_buf ) {
			log("inject: failed to alloc buf");
			exit(0);
		}

		int32_t maxToRead = MAXWARCRECSIZE;
		int32_t toRead = maxToRead;
		s_hasMoreToRead = true;
		if ( s_off + toRead > fsize ) {
			toRead = fsize - s_off;
			s_hasMoreToRead = false;
		}
		int32_t bytesRead = s_file.read ( s_buf , toRead , s_off ) ;
		if ( bytesRead != toRead ) {
			log("inject: read of %s failed at offset "
			    "%"INT64"", s_file.getFilename(), s_off);
			exit(0);
		}
		// null term what we read
		s_buf[bytesRead] = '\0';

		// if not enough to constitute a WARC record probably just new lines
		if( toRead < 20 ) {
			log("inject: done processing file");
			if ( s_outstanding ) {
				log("inject: waiting for socks");return;}
			exit(0);
		}

		// mark the end of what we read
		//char *fend = buf + toRead;

		// point to what we read
		s_pbuf = s_buf;
		s_pbufEnd = s_buf + bytesRead;
	}

 loop:

	char *realStart = s_pbuf;

	// need at least say 100k for warc header
	if ( s_pbuf + 100000 > s_pbufEnd && s_hasMoreToRead )  {
		needReadMore = true;
		goto readMore;
	}

	// find "WARC/1.0" or whatever
	char *whp = s_pbuf;
	for ( ; *whp && strncmp(whp,"WARC/",5) ; whp++ );
	// none?
	if ( ! *whp ) {
		log("inject: could not find WARC/1 header start for file=%s",
		    s_file.getFilename());
		if ( s_outstanding ) {
			log("inject: waiting for socks");return;}
		exit(0);
	}

	char *warcHeader = whp;

	// find end of warc mime HEADER not the content
	char *warcHeaderEnd = strstr(warcHeader,"\r\n\r\n");
	if ( ! warcHeaderEnd ) {
		log("inject: could not find end of WARC header for file=%s.",
		    s_file.getFilename());
		if ( s_outstanding ) {
			log("inject: waiting for socks");return;}
		exit(0);
	}
	// \0 term for strstrs below
	*warcHeaderEnd = '\0';
	//warcHeaderEnd += 4;

	char *warcContent = warcHeaderEnd + 4;

	// get WARC-Type:
	// revisit  (if url was already done before)
	// request (making a GET or DNS request)
	// response (reponse to a GET or dns request)
	// warcinfo (crawling parameters, robots: obey, etc)
	// metadata (fetchTimeMs: 263, hopsFromSeed:P,outlink:)
	char *warcType = strstr(warcHeader,"WARC-Type:");
	if ( ! warcType ) {
		log("inject: could not find WARC-Type:");
		if ( s_outstanding ) {
			log("inject: waiting for socks");return;}
		exit(0);
	}
	warcType += 10;
	for ( ; is_wspace_a(*warcType); warcType++ );

	// get Content-Type: 
	// application/warc-fields (fetch time, hops from seed)
	// application/http; msgtype=request  (the GET request)
	// application/http; msgtype=response (the GET reply)
	char *warcConType = strstr(warcHeader,"Content-Type:");
	if ( ! warcConType ) {
		log("inject: could not find Content-Type:");
		if ( s_outstanding ) {
			log("inject: waiting for socks");return;}
		exit(0);
	}
	warcConType += 13;
	for ( ; is_wspace_a(*warcConType); warcConType++ );
			

	// get Content-Length: of WARC header for its content
	char *warcContentLenStr = strstr(warcHeader,"Content-Length:");
	if ( ! warcContentLenStr ) {
		log("inject: could not find WARC "
		    "Content-Length:");
		if ( s_outstanding ) {
			log("inject: waiting for socks");return;}
		exit(0);
	}
	warcContentLenStr += 15;
	for(;is_wspace_a(*warcContentLenStr);warcContentLenStr++);

	// get warc content len
	int64_t warcContentLen = atoll(warcContentLenStr);

	char *warcContentEnd = warcContent + warcContentLen;

	uint64_t oldOff = s_off;

	uint64_t recSize = (warcContentEnd - realStart); 

	// point to end of this warc record
	s_pbuf += recSize;

	// if we fall outside of the current read buf then re-read
	if ( s_pbuf > s_pbufEnd ) {
		if ( ! s_hasMoreToRead ) {
			log("inject: warc file exceeded file length.");
			if ( s_outstanding ) {
				log("inject: waiting for socks");return;}
			exit(0);
		}
		if ( recSize > MAXWARCRECSIZE ) {
			log("inject: skipping warc file of %"INT64" "
			    "bytes which is too big",recSize);
			s_off += recSize;
		}
		needReadMore = true;
		goto readMore;
	}

	// advance this for next read from the file
	s_off += recSize; // (warcContentEnd - realStart);//s_buf);


	// if WARC-Type: is not response, skip it. so if it
	// is a revisit then skip it i guess.
	if ( strncmp ( warcType,"response", 8 ) ) {
		// read another warc record
		goto loop;
	}

	// warcConType needs to be 
	// application/http; msgtype=response
	if ( strncmp(warcConType,"application/http; msgtype=response", 34) ) {
		// read another warc record
		goto loop;
	}

	char *warcDateStr = strstr(warcHeader,"WARC-Date:");
	if ( warcDateStr ) warcDateStr += 10;
	for(;is_wspace_a(*warcDateStr);warcDateStr++);
	// convert to timestamp
	int64_t warcTime = 0;
	if ( warcDateStr ) warcTime = atotime ( warcDateStr );

	// set the url now
	char *url = strstr(warcHeader,"WARC-Target-URI:");
	if ( url ) url += 16;
	// skip spaces
	for ( ; url && is_wspace_a(*url) ; url++ );
	if ( ! url ) {
		log("inject: could not find WARC-Target-URI:");
		if ( s_outstanding ) {
			log("inject: waiting for socks");return;}
		exit(0);
	}
	// find end of it
	char *urlEnd = url;
	for (;urlEnd&&*urlEnd&&is_urlchar(*urlEnd);urlEnd++);

	// null term url
	//char c = *urlEnd;
	*urlEnd = '\0';


	char *httpReply = warcContent;
	int64_t httpReplySize = warcContentLen;

	// sanity check
	//char *bufEnd = s_buf + MAXWARCRECSIZE;
	if ( httpReply + httpReplySize >= s_pbufEnd ) {
		int needMore = httpReply + httpReplySize - s_pbufEnd;
		log("inject: not reading enough content to inject "
		    "url %s . increase MAXWARCRECSIZE by %"INT32" more",url,
		    needMore);
		exit(0);
	}

	// put it back
	//*urlEnd = c;


	// should be a mime that starts with GET or POST
	HttpMime m;
	if ( ! m.set ( httpReply , httpReplySize , NULL ) ) {
	  // 	if ( httpReplySize > 128 ) httpReplySize = 128;
	// 	httpReply [ httpReplySize ] = '\0';
	// 	log("build: inject: Failed to set mime at offset "
	// 	    "%"INT64" where request=%s",s_off,httpReply);
		log("inject: failed to set http mime at %"INT64" in file"
		    ,oldOff);
		goto loop;
	// 	exit(0);
	}

	// check content type
	int ct = m.getContentType();
	if ( ct != CT_HTML &&
	     ct != CT_TEXT &&
	     ct != CT_XML &&
	     ct != CT_JSON ) {
		goto loop;
	}


	SafeBuf req;

	// a different format?
	char *ipStr = "1.2.3.4";
	req.safePrintf(
		       "POST /admin/inject HTTP/1.0\r\n"
		       "Content-Length: 000000000\r\n"//bookmrk
		       "Content-Type: text/html\r\n"
		       "Connection: Close\r\n"
		       "\r\n"
		       // we need this ?
		       "?"
		       "c=%s&"
		       // do parsing consistency testing (slower!)
		       //"dct=1&"
		       "hasmime=1&"
		       // prevent looking up firstips
		       // on all outlinks for speed:
		       "spiderlinks=0&"
		       "quick=1&" // quick reply
		       "dontlog=0&"

		       // do not do re-injects. should save a TON of time
		       "newonly=1&"
			      
		       "lastspidered=%"INT64"&"
		       "firstindexed=%"INT64"&"

		       "deleteurl=0&"
		       "ip=%s&"
		       //"recycle=%"INT32"&"
		       //"delete=%"INT32"&"
		       "u="
		       ,s_coll

		       ,warcTime
		       ,warcTime
		       
		       ,ipStr
		       //recycle,
		       );

	// url encode the url
	req.urlEncode ( url );
	// finish it up
	req.safePrintf("&content=");
	// store the content after the &ucontent
	req.urlEncode ( httpReply , httpReplySize );
	req.nullTerm();


	// replace 00000 with the REAL content length
	char *start = strstr(req.getBufStart(),"c=");
	int32_t realContentLen = gbstrlen(start);
	char *ptr = req.getBufStart() ;
	// find start of the 9 zeroes
	while ( *ptr != '0' || ptr[1] !='0' ) ptr++;
	// store length there
	sprintf ( ptr , "%09"UINT32"" , realContentLen );
	// remove the \0
	ptr += strlen(ptr); *ptr = '\r';


	int32_t ip = s_ip;
	int32_t port = s_port;

	// try hosts.conf
	if ( ip == 0 ) {
		// round robin over hosts in s_hosts2
		if ( s_rrn >= s_hosts2.getNumHosts() ) s_rrn = 0;
		Host *h = s_hosts2.getHost ( s_rrn );
		ip = h->m_ip;
		port = h->m_httpPort;
		s_rrn++;
	}

	// log it
	log("inject: injecting to %s:%i WARC url %s",iptoa(ip),(int)port,url);

	// now inject it
	bool status = s_tcp.sendMsg( NULL, 0, ip, port, req.getBufStart(), req.getCapacity(), req.length(),
								 req.length(), NULL, injectedWrapper,
								 // because it seems some sockets get stuck and
								 // they have no reply but the host they are
								 // connected to no longer has the connection
								 // open. and the readbuf is empty, but the send
								 // buf has been sent and it appears the inject
								 // when through. just the reply was never
								 // sent back for some reason.
								 5 * 60 * 1000, // timeout, 5 mins
								 -1, -1 );

	int realMax = 10;
	if ( s_hosts2.getNumHosts() > 1 )
		realMax = s_hosts2.getNumHosts() * 2;

	// launch another if blocked
	if ( ! status ) {
		// let injectedWrapper() below free it
		req.detachBuf();
		//int32_t nh = g_hostdb.getNumHosts();
		//nh = (nh * 15) / 10;
		//if ( nh > MAX_INJECT_SOCKETS - 10 ) 
		//	nh = MAX_INJECT_SOCKETS - 10;
		//if ( nh < 5 ) nh = 5;
		// limit to one socket right now
		//if ( ++s_outstanding < 1 ) goto loop;
		s_outstanding++;
		if ( s_outstanding < MAX_INJECT_SOCKETS &&
		     s_outstanding < realMax ) 
		  goto loop;
		return;
	}
		
	if ( g_errno ) {
		// let tcpserver.cpp free it
		req.detachBuf();
		log("build: inject had error: %s.",mstrerror(g_errno));
	}
	// loop if not
	goto loop;
}


void doInjectArc ( int64_t fsize ) {

	static char *s_buf = NULL;

	static bool s_hasMoreToRead;

	static char *s_pbuf = NULL;
	static char *s_pbufEnd = NULL;

	bool needReadMore = false;
	if ( ! s_pbuf ) needReadMore = true;


 readMore:

	if ( needReadMore ) {

		log("inject: reading %"INT64" bytes more of arc file"
		    ,(int64_t)MAXWARCRECSIZE);

		// are we done?
		if ( s_off >= fsize ) { 
			log("inject: done parsing arc file");
			if ( s_outstanding ) {
				log("inject: waiting for socks");return;}
			g_loop.reset();  
			exit(0); 
		}

		// read 1MB of data into this buf to get the first WARC record
		// it must be < 1MB or we faulter.
		if ( ! s_buf ) {
			int64_t need = MAXWARCRECSIZE + 1;
			s_buf = (char *)mmalloc ( need ,"sibuf");
		}
		if ( ! s_buf ) {
			log("inject: failed to alloc buf");
			exit(0);
		}

		int32_t maxToRead = MAXWARCRECSIZE;
		int32_t toRead = maxToRead;
		s_hasMoreToRead = true;
		if ( s_off + toRead > fsize ) {
			toRead = fsize - s_off;
			s_hasMoreToRead = false;
		}
		int32_t bytesRead = s_file.read ( s_buf , toRead , s_off ) ;
		if ( bytesRead != toRead ) {
			log("inject: read of %s failed at offset "
			    "%"INT64"", s_file.getFilename(), s_off);
			exit(0);
		}
		// null term what we read
		s_buf[bytesRead] = '\0';

		// if not enough to constitute a ARC record probably just new 
		// lines
		if( toRead < 20 ) {
			log("inject: done processing file");
			if ( s_outstanding ) {
				log("inject: waiting for socks");return;}
			exit(0);
		}

		// mark the end of what we read
		//char *fend = buf + toRead;

		// point to what we read
		s_pbuf = s_buf;
		s_pbufEnd = s_buf + bytesRead;
	}

 loop:

	char *realStart = s_pbuf;

	// need at least say 100k for arc header
	if ( s_pbuf + 100000 > s_pbufEnd && s_hasMoreToRead )  {
		needReadMore = true;
		goto readMore;
	}

	// find \n\nhttp://
	char *whp = s_pbuf;
	for ( ; *whp ; whp++ ) {
		if ( whp[0] != '\n' ) continue;
		if ( strncmp(whp+1,"http://",7) ) continue;
		break;
	}
	// none?
	if ( ! *whp ) {
		log("inject: could not find next \\nhttp:// in arc file");
		if ( s_outstanding ) {log("inject: waiting for socks");return;}
		exit(0);
	}

	char *arcHeader = whp;

	// find end of arc header not the content
	char *arcHeaderEnd = strstr(arcHeader+1,"\n");
	if ( ! arcHeaderEnd ) {
		log("inject: could not find end of ARC header.");
		exit(0);
	}
	// \0 term for strstrs below
	*arcHeaderEnd = '\0';

	char *arcContent = arcHeaderEnd + 1;

	// parse arc header line
	char *url = arcHeader + 1;
	char *hp = url;

	for ( ; *hp && *hp != ' ' ; hp++ );
	if ( ! *hp ) {log("inject: bad arc header 1.");exit(0);}
	*hp++ = '\0';
	char *ipStr = hp;


	for ( ; *hp && *hp != ' ' ; hp++ );
	if ( ! *hp ) {log("inject: bad arc header 2.");exit(0);}
	*hp++ = '\0';
	char *timeStr = hp;


	for ( ; *hp && *hp != ' ' ; hp++ );
	if ( ! *hp ) {log("inject: bad arc header 3.");exit(0);}
	*hp++ = '\0'; // null term timeStr
	char *arcConType = hp;

	for ( ; *hp && *hp != ' ' ; hp++ );
	if ( ! *hp ) {log("inject: bad arc header 4.");exit(0);}
	*hp++ = '\0'; // null term arcContentType

	char *arcContentLenStr = hp;
	// this is already \0 terminated from above!
	//for ( ; *hp && *hp != '\n' ; hp++ );
	//if ( ! *hp ) {log("inject: bad arc header 5.");exit(0);}
	//*hp++ = '\0'; // null term lenStr

	

	// get arc content len
	int64_t arcContentLen = atoll(arcContentLenStr);

	char *arcContentEnd = arcContent + arcContentLen;

	//uint64_t oldOff = s_off;

	uint64_t recSize = (arcContentEnd - realStart); 

	// point to end of this arc record
	s_pbuf += recSize;

	// if we fall outside of the current read buf then re-read
	if ( s_pbuf > s_pbufEnd ) {
		if ( ! s_hasMoreToRead ) {
			log("inject: arc file exceeded file length.");
			if ( s_outstanding ) {
				log("inject: waiting for socks");return;}
			exit(0);
		}
		if ( recSize > MAXWARCRECSIZE ) {
			log("inject: skipping arc file of %"INT64" "
			    "bytes which is too big",recSize);
			s_off += recSize;
		}
		needReadMore = true;
		goto readMore;
	}

	// advance this for next read from the file
	s_off += recSize;


	// arcConType needs to indexable
	int32_t ct = getContentTypeFromStr ( arcConType );
	if ( ct != CT_HTML &&
	     ct != CT_TEXT &&
	     ct != CT_XML &&
	     ct != CT_JSON ) {
		// read another arc record
		goto loop;
	}

	// convert to timestamp
	int64_t arcTime = 0;
	// this time structure, once filled, will help yield a time_t
	struct tm t;
	// DAY OF MONTH
	t.tm_mday = atol2 ( timeStr + 6 , 2 );
	// MONTH
	t.tm_mon = atol2 ( timeStr + 4  , 2 );
	// YEAR
	t.tm_year = atol2 ( timeStr     , 4 ) - 1900 ; // # of years since 1900
	// TIME
	t.tm_hour = atol2 ( timeStr +  8 , 2 );
	t.tm_min  = atol2 ( timeStr + 10 , 2 );
	t.tm_sec  = atol2 ( timeStr + 12 , 2 );
	// unknown if we're in  daylight savings time
	t.tm_isdst = -1;
	// translate using mktime
	arcTime = timegm ( &t );


	char *httpReply = arcContent;
	int64_t httpReplySize = arcContentLen;

	// sanity check
	if ( httpReply + httpReplySize >= s_pbufEnd ) {
		int needMore = httpReply + httpReplySize - s_pbufEnd;
		log("inject: not reading enough content to inject "
		    "url %s . increase MAXWARCRECSIZE by %"INT32" more",url,
		    needMore);
		exit(0);
	}


	SafeBuf req;

	// a different format?
	//char *ipStr = "1.2.3.4";
	req.safePrintf(
		       "POST /admin/inject HTTP/1.0\r\n"
		       "Content-Length: 000000000\r\n"//bookmrk
		       "Content-Type: text/html\r\n"
		       "Connection: Close\r\n"
		       "\r\n"
		       // we need this ?
		       "?"
		       "c=%s&"
		       // do parsing consistency testing (slower!)
		       //"dct=1&"
		       "hasmime=1&"
		       // prevent looking up firstips
		       // on all outlinks for speed:
		       "spiderlinks=0&"
		       "quick=1&" // quick reply
		       "dontlog=0&"

		       // do not do re-injects. should save a TON of time
		       "newonly=1&"
			      
		       "lastspidered=%"INT64"&"
		       "firstindexed=%"INT64"&"

		       "deleteurl=0&"
		       "ip=%s&"
		       //"recycle=%"INT32"&"
		       //"delete=%"INT32"&"
		       "u="
		       ,s_coll

		       ,arcTime
		       ,arcTime
		       
		       ,ipStr
		       //recycle,
		       );

	// url encode the url
	req.urlEncode ( url );
	// finish it up
	req.safePrintf("&content=");
	// store the content after the &ucontent
	req.urlEncode ( httpReply , httpReplySize );
	req.nullTerm();


	// replace 00000 with the REAL content length
	char *start = strstr(req.getBufStart(),"c=");
	int32_t realContentLen = gbstrlen(start);
	char *ptr = req.getBufStart() ;
	// find start of the 9 zeroes
	while ( *ptr != '0' || ptr[1] !='0' ) ptr++;
	// store length there
	sprintf ( ptr , "%09"UINT32"" , realContentLen );
	// remove the \0
	ptr += strlen(ptr); *ptr = '\r';


	int32_t ip = s_ip;
	int32_t port = s_port;

	// try hosts.conf
	if ( ip == 0 ) {
		// round robin over hosts in s_hosts2
		if ( s_rrn >= s_hosts2.getNumHosts() ) s_rrn = 0;
		Host *h = s_hosts2.getHost ( s_rrn );
		ip = h->m_ip;
		port = h->m_httpPort;
		s_rrn++;
	}

	// log it
	log("inject: injecting ARC %s to %s:%i contentLen=%"INT64""
	    ,url
	    ,iptoa(ip)
	    ,(int)port
	    ,arcContentLen);

	// now inject it
	bool status = s_tcp.sendMsg( NULL, 0, ip, port, req.getBufStart(), req.getCapacity(), req.length(),
								 req.length(), NULL, injectedWrapper,
								 // because it seems some sockets get stuck and
								 // they have no reply but the host they are
								 // connected to no longer has the connection
								 // open. and the readbuf is empty, but the send
								 // buf has been sent and it appears the inject
								 // when through. just the reply was never
								 // sent back for some reason.
								 5 * 60 * 1000, // timeout, 5 mins
								 -1, -1 );

	int realMax = 10;
	if ( s_hosts2.getNumHosts() > 1 )
		realMax = s_hosts2.getNumHosts() * 3;

	// launch another if blocked
	if ( ! status ) {
		// let injectedWrapper() below free it
		req.detachBuf();
		//int32_t nh = g_hostdb.getNumHosts();
		//nh = (nh * 15) / 10;
		//if ( nh > MAX_INJECT_SOCKETS - 10 ) 
		//	nh = MAX_INJECT_SOCKETS - 10;
		//if ( nh < 5 ) nh = 5;
		// limit to one socket right now
		//if ( ++s_outstanding < 1 ) goto loop;
		s_outstanding++;
		if ( s_outstanding < MAX_INJECT_SOCKETS &&
		     s_outstanding < realMax ) 
		  goto loop;
		return;
	}
		
	if ( g_errno ) {
		// let tcpserver.cpp free it
		req.detachBuf();
		log("build: inject had error: %s.",mstrerror(g_errno));
	}
	// loop if not
	goto loop;
}


void injectedWrapper ( void *state , TcpSocket *s ) {
	s_outstanding--;

	// wtf is this? s_tcp is counting THIS socket so say "== 1"
	if ( s_tcp.m_numUsed == 1 && s_outstanding > 0 ) {
		log("inject: resetting s_outstanding to 0");
		s_outstanding = 0;
	}

	// debug note
	logf(LOG_DEBUG,"inject: out=%i used=%i",(int)s_outstanding,(int)s_tcp.m_numUsed);

	// errno?
	if ( g_errno ) {
		log("inject: Got server error: %s.",
		    mstrerror(g_errno));
		doInject(0,NULL);
		return;
	}
	// free send buf
	char *req    = s->m_sendBuf;
	int32_t  reqAlloc = s->m_sendBufSize;
	mfree ( req , reqAlloc , "maininject");
	s->m_sendBuf = NULL;

	int32_t i;
	static int32_t s_last = 0;
	int32_t now = getTimeLocal();

	// save docid every 10 seconds
	if ( now - s_last > 10 ) {
		int64_t minDocId = 0x0000ffffffffffffLL;
		// get min outstanding docid inject request
		for ( i = 0 ; i < MAX_INJECT_SOCKETS ; i++ ) {
			// skip if occupied
			if ( ! s_req[i] ) continue;
			if ( s_docId[i] < minDocId ) minDocId = s_docId[i];
		}
		// map it
		bool saveIt = false;
		// are we the min?
		int32_t i; for ( i = 0 ; i < MAX_INJECT_SOCKETS ; i++ ) {
			// skip if occupied
			if ( s_req[i] != req ) continue;
			// we got our request
			if ( s_docId[i] == minDocId ) saveIt = true;
			break;
		}
		if ( saveIt ) {
			s_last = now;
			SafeBuf sb;
			sb.safePrintf("%"INT64"\n",minDocId);
			char fname[256];
			//sprintf(fname,"%s/lastinjectdocid.dat",g_hostdb.m_dir
			sprintf(fname,"./lastinjectdocid.dat");
			sb.dumpToFile(fname);
		}
	}

	// remove ourselves from map
	for ( i = 0 ; i < MAX_INJECT_SOCKETS ; i++ ) 
		if ( s_req[i] == req ) s_req[i] = NULL;

	// get return code
	char *reply = s->m_readBuf;
	logf(LOG_INFO,"inject: reply=\"%s\"",reply);
	doInject(0,NULL);
}

void saveRdbs ( int fd , void *state ) {
	int64_t now = gettimeofdayInMilliseconds_force();
	int64_t last;
	Rdb *rdb ;
	// . try saving every 10 minutes from time of last write to disk
	// . if nothing more added to tree since then, Rdb::close() return true
	//int64_t delta = 10LL*60LL*1000LL;
	// . this is in MINUTES
	int64_t delta = (int64_t)g_conf.m_autoSaveFrequency *60000LL;
	if ( delta <= 0 ) return;
	// jitter it up a bit so not all hostIds save at same time, 15 secs
	delta += (int64_t)(g_hostdb.m_hostId % 10) * 15000LL + (rand()%7500);
	rdb = g_tagdb.getRdb();
	last = rdb->getLastWriteTime();
	if ( now - last > delta )
		if ( ! rdb->close(NULL,NULL,false,false)) return;
	rdb = g_posdb.getRdb();
	last = rdb->getLastWriteTime();
	if ( now - last > delta )
		if ( ! rdb->close(NULL,NULL,false,false)) return;
	//rdb = g_datedb.getRdb();
	//last = rdb->getLastWriteTime();
	//if ( now - last > delta )
	//	if ( ! rdb->close(NULL,NULL,false,false)) return;
	rdb = g_titledb.getRdb();
	last = rdb->getLastWriteTime();
	if ( now - last > delta )
		if ( ! rdb->close(NULL,NULL,false,false)) return;
	rdb = g_spiderdb.getRdb();
	last = rdb->getLastWriteTime();
	if ( now - last > delta )
		if ( ! rdb->close(NULL,NULL,false,false)) return;
	rdb = g_clusterdb.getRdb();
	last = rdb->getLastWriteTime();
	if ( now - last > delta )
		if ( ! rdb->close(NULL,NULL,false,false)) return;
	rdb = g_statsdb.getRdb();
	last = rdb->getLastWriteTime();
	if ( now - last > delta )
		if ( ! rdb->close(NULL,NULL,false,false)) return;
}

// JAB: warning abatement
#if 0
bool checkDataParity ( ) {
	//return true;
	g_threads.disableThreads();

	// test the first collection
	char *coll = g_collectiondb.getCollName ( 0 );

	Msg5 msg5;
	Msg5 msg5b;
	RdbList list;
	key_t startKey;
	key_t endKey;
	startKey.setMin();
	endKey.setMax();
	//int32_t minRecSizes = 64000;
	
	// CHECK INDEXDB
	log ( LOG_INFO, "db: Verifying Indexdb..." );
	if ( ! msg5.getList ( RDB_INDEXDB   ,
			      coll          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      64000         , // minRecSizes   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         ))// err correction?
		return log("db: HEY! it did not block");

	int32_t count = 0;
	int32_t got   = 0;
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k = list.getCurrentKey();
		// skip negative keys
		if ( (k.n0 & 0x01) == 0x00 ) continue;
		count++;
		//uint32_t groupId = k.n1 & g_hostdb.m_groupMask;
		uint32_t shardNum = getShardNum ( RDB_INDEXDB, &k );
		if ( groupId == g_hostdb.m_groupId ) got++;
	}
	if ( got != count ) {
		log ("db: Out of first %"INT32" records in indexdb, only %"INT32" belong "
		     "to our group.",count,got);
		// exit if NONE, we probably got the wrong data
		if ( got == 0 ) return log("db: Are you sure you have the "
					   "right "
					   "data in the right directory? "
					   "Exiting.");
		return log ( "db: Exiting due to Indexdb inconsistency." );
	}
	log ( LOG_INFO, "db: Indexdb passed verification successfully. (%"INT32")",
			count );
	// CHECK TITLEDB
	log ( LOG_INFO, "db: Verifying Titledb..." );
	if ( ! msg5.getList ( RDB_TITLEDB   ,
			      coll          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      1024*1024     , // minRecSizes   ,
			      true          , // includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      0             , // startFileNum  ,
			      -1            , // numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         , // err correction?
			      NULL          , // cache key ptr
			      0             , // retry num
			      -1            , // maxRetries
			      true          , // compensate for merge
			      -1LL          , // sync point
			      &msg5b        ))
		return log("db: HEY! it did not block");

	count = 0;
	got   = 0;
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k = list.getCurrentKey();
		// skip negative keys
		if ( (k.n0 & 0x01) == 0x00 ) continue;
		count++;
		uint32_t shardNum = getShardNum ( RDB_TITLEDB , &k );
		//int32_t groupId = k.n1 & g_hostdb.m_groupMask;
		if ( groupId == g_hostdb.m_groupId ) got++;
	}
	if ( got != count ) {
		log ("db: Out of first %"INT32" records in titledb, only %"INT32" belong "
		     "to our group.",count,got);
		// exit if NONE, we probably got the wrong data
		if ( count > 10 && got == 0 ) 
			return log("db: Are you sure you have the right "
				   "data in the right directory? "
				   "Exiting.");
		return log ( "db: Exiting due to Titledb inconsistency." );
	}

	log ( LOG_INFO, "db: Titledb passed verification successfully. (%"INT32")",
			count );

	// DONE
	g_threads.enableThreads();
	return true;
}
#endif

bool shutdownOldGB ( int16_t port ) {
	log("db: Saving and shutting down the other gb process." );
	// now make a new socket descriptor
	int sd = socket ( AF_INET , SOCK_STREAM , 0 ) ;
	// return NULL and set g_errno on failure
	if ( sd <  0 ) {
		// copy errno to g_errno
		g_errno = errno;
		log("tcp: Failed to create new socket: %s.",
		    mstrerror(g_errno));
		return false;
	}
	struct sockaddr_in to;
	to.sin_family = AF_INET;
	// our ip's are always in network order, but ports are in host order
	to.sin_addr.s_addr =  atoip("127.0.0.1",9);
	to.sin_port        =  htons((uint16_t)port);
	bzero ( &(to.sin_zero) , 8 ); // TODO: bzero too slow?
	// note it
	log("db: Connecting to port %hu.",port);
	// connect to the socket. This should block until it does
 again:
	if ( ::connect ( sd, (sockaddr *)&to, sizeof(to) ) != 0 ) {
		if ( errno == EINTR ) goto again;
		return log("admin: Got connect error: %s.",mstrerror(errno));
	}
	// note it
	log("db: Connected. Issuing shutdown command.");
	// send the message
	char *msg = "GET /master?usave=1 HTTP/1.0\r\n\r\n";
	write ( sd , msg , gbstrlen(msg) );
	// wait for him to shut down the socket
	char rbuf [5000];
	int32_t n;
 readmore:
	errno = 0;
	n = read ( sd , rbuf, 5000 );
	if ( n == -1 && errno == EINTR ) goto readmore;
	if ( n == -1 )
		return log("db: Got error reading reply: %s.",
			   mstrerror(errno));
	// success...
	close(sd);
	log("db: Received reply from old gb process.");
	return true;
}

bool memTest() {
	// let's ensure our core file can dump
	struct rlimit lim;
	lim.rlim_cur = lim.rlim_max = RLIM_INFINITY;
	if ( setrlimit(RLIMIT_CORE,&lim) )
		log("db: setrlimit: %s.", mstrerror(errno) );

	void *ptrs[4096];
	int numPtrs=0;
	//int totalMem=0;
	int i;
	// if ( ! g_log.init( "./memlog" ) ) {//g_hostdb.m_logFilename )        ) {
	// 	fprintf (stderr,"db: Log file init failed.\n" ); return 1; }
	//g_mem.init(0xffffffff);
	g_conf.m_maxMem = 0xffffffffLL;
	g_mem.init( );//g_mem.m_maxMem );
	

	fprintf(stderr, "memtest: Testing memory bus bandwidth.\n");
	// . read in 20MB 100 times (~2GB total)
	// . tests main memory throughput
	fprintf(stderr, "memtest: Testing main memory.\n");
	membustest ( 20*1024*1024 , 100 , true );
	// . read in 1MB 2,000 times (~2GB)
	// . tests the L2 cache
	fprintf(stderr, "memtest: Testing 1MB L2 cache.\n");
	membustest ( 1024*1024 , 2000 , true );
	// . read in 8000 200,000 times (~1.6GB)
	// . tests the L1 cache
	fprintf(stderr, "memtest: Testing 8KB L1 cache.\n");
	membustest ( 8000 , 100000 , true );

	fprintf(stderr, "memtest: Allocating up to %"INT64" bytes\n",
		g_conf.m_maxMem);
	for (i=0;i<4096;i++) {
		ptrs[numPtrs] = mmalloc(1024*1024, "memtest");
		if (!ptrs[numPtrs]) break;
		numPtrs++;
	}

	fprintf(stderr, "memtest: Was able to allocate %"INT64" bytes of a "
		"total of "
	    "%"INT64" bytes of memory attempted.\n",
	    g_mem.m_used,g_conf.m_maxMem);

	return true;
}


// . read in "nb" bytes, loops times, 
// . if readf is false, do write test, not read test
void membustest ( int32_t nb , int32_t loops , bool readf ) {

	int32_t count = loops;

	// don't exceed 50NB
	if ( nb > 50*1024*1024 ) {
		fprintf(stderr,"memtest: truncating to 50 Megabytes.\n");
		nb = 50*1024*1024;
	}

	int32_t n = nb ; //* 1024 * 1024 ;

	// make n divisble by 64
	//int32_t rem = n % 64;
	//if ( rem > 0 ) n += 64 - rem;

	// get some memory, 4 megs
	//#undef malloc
	//register char *buf = (char *)malloc(n + 64);
	//#define malloc coreme
	int32_t bufSize = 50*1024*1024;
	register char *buf = (char *) mmalloc ( bufSize , "main" );
	if ( ! buf ) return;
	char *bufStart = buf;
	register char *bufEnd = buf + n;

	//fprintf(stderr,"pre-reading %"INT32" NB \n",nb);
	// pre-read it so sbrk() can do its thing
	for ( int32_t i = 0 ; i < n ; i++ ) buf[i] = 1;

	// time stamp
	int64_t t = gettimeofdayInMilliseconds_force();

	fprintf(stderr,"memtest: start = %"INT64"\n",t);

	// . time the read loop
	// . each read should only be 2 assenbly movl instructions:
	//   movl	-52(%ebp), %eax
	//   movl	(%eax), %eax
	//   movl	-52(%ebp), %eax
	//   movl	4(%eax), %eax
	//   ...
 loop:
	register int32_t c;

	if ( readf ) {
		while ( buf < bufEnd ) {
			// repeat 16x for efficiency.limit comparison to bufEnd
			c = *(int32_t *)(buf+ 0);
			c = *(int32_t *)(buf+ 4);
			c = *(int32_t *)(buf+ 8);
			c = *(int32_t *)(buf+12);
			c = *(int32_t *)(buf+16);
			c = *(int32_t *)(buf+20);
			c = *(int32_t *)(buf+24);
			c = *(int32_t *)(buf+28);
			c = *(int32_t *)(buf+32);
			c = *(int32_t *)(buf+36);
			c = *(int32_t *)(buf+40);
			c = *(int32_t *)(buf+44);
			c = *(int32_t *)(buf+48);
			c = *(int32_t *)(buf+52);
			c = *(int32_t *)(buf+56);
			c = *(int32_t *)(buf+60);
			buf += 64;
		}
	}
	else {
		while ( buf < bufEnd ) {
			// repeat 8x for efficiency. limit comparison to bufEnd
			*(int32_t *)(buf+ 0) = 0;
			*(int32_t *)(buf+ 4) = 1;
			*(int32_t *)(buf+ 8) = 2;
			*(int32_t *)(buf+12) = 3;
			*(int32_t *)(buf+16) = 4;
			*(int32_t *)(buf+20) = 5;
			*(int32_t *)(buf+24) = 6;
			*(int32_t *)(buf+28) = 7;
			buf += 32;
		}
	}
	if ( --count > 0 ) {
		buf = bufStart;
		goto loop;
	}

	// completed
	int64_t now = gettimeofdayInMilliseconds_force();
	fprintf(stderr,"memtest: now = %"INT64"\n",t);
	// multiply by 4 since these are int32_ts
	char *op = "read";
	if ( ! readf ) op = "wrote";
	fprintf(stderr,"memtest: %s %"INT32" bytes (x%"INT32") in"
		"%"UINT64" ms.\n",
		 op , n , loops , now - t );
	// stats
	if ( now - t == 0 ) now++;
	double d = (1000.0*(double)loops*(double)(n)) / ((double)(now - t));
	fprintf(stderr,"memtest: we did %.2f MB/sec.\n" , d/(1024.0*1024.0));

	mfree ( bufStart , bufSize , "main" );

	return ;
}


bool cacheTest() {

	g_conf.m_maxMem = 2000000000LL; // 2G
	//g_mem.m_maxMem  = 2000000000LL; // 2G

	hashinit();

	// use an rdb cache
	RdbCache c;
	// init, 50MB
	int32_t maxMem = 50000000; 
	// . how many nodes in cache tree can we fit?
	// . each rec is key (12) and ip(4)
	// . overhead in cache is 56
	// . that makes 56 + 4 = 60
	// . not correct? stats suggest it's less than 25 bytes each
	int32_t maxCacheNodes = maxMem / 25;
	// set the cache
	if ( ! c.init ( maxMem        ,
			4             ,  // fixed data size of rec
			false         ,  // support lists of recs?
			maxCacheNodes ,
			false         ,  // use half keys?
			"cachetest"        ,  // dbname
			false         )) // save cache to disk?
		return log("test: Cache init failed.");

	int32_t numRecs = 0 * maxCacheNodes;
	logf(LOG_DEBUG,"test: Adding %"INT32" recs to cache.",numRecs);

	// timestamp
	int32_t timestamp = 42;
	// keep ring buffer of last 10 keys
	key_t oldk[10];
	int32_t  oldip[10];
	int32_t  b = 0;
	// fill with random recs
	for ( int32_t i = 0 ; i < numRecs ; i++ ) {
		if ( (i % 100000) == 0 )
			logf(LOG_DEBUG,"test: Added %"INT32" recs to cache.",i);
		// random key
		key_t k ;
		k.n1 = rand();
		k.n0 = rand();
		k.n0 <<= 32;
		k.n0 |= rand();
		int32_t ip = rand();
		// keep ring buffer
		oldk [b] = k;
		oldip[b] = ip;
		if ( ++b >= 10 ) b = 0;
		// make rec,size, like dns, will be 4 byte hash and 4 byte key?
		c.addRecord((collnum_t)0,k,(char *)&ip,4,timestamp);
		// reset g_errno in case it had an error (we don't care)
		g_errno = 0;	
		// get a rec too!
		if ( i < 10 ) continue;
		int32_t next = b + 1;
		if ( next >= 10 ) next = 0;
		key_t back = oldk[next];
		char *rec;
		int32_t  recSize;
		if ( ! c.getRecord ( (collnum_t)0 ,
				     back         ,
				     &rec     ,
				     &recSize ,
				     false    ,  // do copy?
				     -1       ,  // maxAge   ,
				     true     , // inc count?
				     NULL     , // *cachedTime = NULL,
				     true     )){ // promoteRecord?
			char *xx= NULL; *xx = 0; }
		if ( ! rec || recSize != 4 || *(int32_t *)rec != oldip[next] ) {
			char *xx= NULL; *xx = 0; }
	}		     		

	// now try variable sized recs
	c.reset();

	logf(LOG_DEBUG,"test: Testing variably-sized recs.");

	// init, 300MB
	maxMem = 300000000; 
	// . how many nodes in cache tree can we fit?
	// . each rec is key (12) and ip(4)
	// . overhead in cache is 56
	// . that makes 56 + 4 = 60
	// . not correct? stats suggest it's less than 25 bytes each
	maxCacheNodes = maxMem / 5000;
	//maxCacheNodes = 1200;
	// set the cache
	if ( ! c.init ( maxMem        ,
			-1            ,  // fixed data size of rec
			false         ,  // support lists of recs?
			maxCacheNodes ,
			false         ,  // use half keys?
			"cachetest"        ,  // dbname
			false         )) // save cache to disk?
		return log("test: Cache init failed.");

	numRecs = 30 * maxCacheNodes;
	//numRecs = 2 * maxCacheNodes;
	logf(LOG_DEBUG,"test: Adding %"INT32" recs to cache.",numRecs);

	// timestamp
	timestamp = 42;
	// keep ring buffer of last 10 keys
	int32_t oldrs[10];
	b = 0;
	//char lastp;
	// rec to add
	char *rec;
	int32_t  recSize;
	int32_t  maxRecSize = 40000000; // 40MB for termlists
	int32_t  numMisses = 0;
	char *buf = (char *)mmalloc ( maxRecSize + 64 ,"cachetest" );
	if ( ! buf ) return false;
	//sleep(2);
	// fill with random recs
	for ( int32_t i = 0 ; i < numRecs ; i++ ) {
		if ( (i % 100) == 0 )
			logf(LOG_DEBUG,"test: Added %"INT32" recs to cache. "
			     "Misses=%"INT32".",i,numMisses);
		// random key
		key_t k ;
		k.n1 = rand();
		k.n0 = rand();
		k.n0 <<= 32;
		k.n0 |= rand();
		// random size
		recSize = rand()%maxRecSize;//100000;
		// keep ring buffer
		oldk [b] = k;
		oldrs[b] = recSize;
		//oldip[b] = ip;
		if ( ++b >= 10 ) b = 0;
		// make the rec
		rec = buf;
		memset ( rec , (char)k.n1, recSize );
		//log("test: v0");
		// make rec,size, like dns, will be 4 byte hash and 4 byte key?
		if ( ! c.addRecord((collnum_t)0,k,rec,recSize,timestamp) ) {
			char *xx=NULL; *xx=0; }
		// do a dup add 1% of the time
		if ( (i % 100) == 0 )
			if(!c.addRecord((collnum_t)0,k,rec,recSize,timestamp)){
				char *xx=NULL; *xx=0; }
		//log("test: v1");
		//c.verify();
		// reset g_errno in case it had an error (we don't care)
		g_errno = 0;	
		// get a rec too!
		if ( i < 10 ) continue;
		int32_t next = b + 1;
		if ( next >= 10 ) next = 0;
		key_t back = oldk[next];
		//log("cache: get rec");
		if ( ! c.getRecord ( (collnum_t)0 ,
				     back         ,
				     &rec     ,
				     &recSize ,
				     false    ,  // do copy?
				     -1       ,  // maxAge   ,
				     true     , // inc count?
				     NULL     , // *cachedTime = NULL,
				     true) ) {//true     )){ // promoteRecord?
			numMisses++;
			//logf(LOG_DEBUG,"test: missed"); 
			continue;
			char *xx= NULL; 
			*xx = 0; 
		}
		//log("cache: got rec");
		//char *p = c.m_bufs[0] + 9210679 + 51329;
		//if ( *p != lastp ) 
		//	logf(LOG_DEBUG,"test: p changed");
		//lastp = *p;
		if ( recSize != oldrs[next] ) {
			logf(LOG_DEBUG,"test: bad rec size.");
			char *xx=NULL; *xx = 0;
			continue;
		}
		char r = (char)back.n1;
		for ( int32_t j = 0 ; j < recSize ; j++ ) {
			if ( rec[j] == r ) continue;
			logf(LOG_DEBUG,"test: bad char in rec.");
			char *xx=NULL; *xx = 0;
		}
		//if ( ! rec || recSize != 4 || *(int32_t *)rec != oldip[next] ) {
		//	char *xx= NULL; *xx = 0; }
	}		     		

	c.verify();

	c.reset();

	return true;
}

bool ramdiskTest() {

	//g_conf.m_maxMem = 2000000000LL; // 2G
	//g_mem.m_maxMem  = 2000000000LL; // 2G

	//hashinit();

	int fd = open ("/dev/ram2",O_RDWR);

	if ( fd < 0 ) {
		fprintf(stderr,"ramdisk: failed to open /dev/ram2\n");
		return false;
	}

	char *buf[1000];
	gbpwrite ( fd , buf , 1000, 0 );

	close ( fd);
	return true;
}

// CountDomains Structures and function definitions
struct lnk_info {
	char          *dom;
	int32_t           domLen;
	int32_t           pages;
};

struct dom_info {
	char          *dom;
	int32_t           domLen;
	int32_t           dHash;
	int32_t           pages;
	//int64_t      quality;
	struct ip_info 	      **ip_list;
	int32_t           numIp;		
	//HashTable     *dht;
	int32_t 	      *lnk_table;
	int32_t           tableSize;
	int32_t           lnkCnt;
	int32_t	       lnkPages;
};

struct ip_info {
	uint32_t  ip;
	int32_t           pages;
	//int64_t      quality;
	struct dom_info **dom_list;
	int32_t           numDom;
};

static int ip_fcmp  (const void *p1, const void *p2);
static int ip_dcmp  (const void *p1, const void *p2);

static int dom_fcmp (const void *p1, const void *p2);
static int dom_lcmp (const void *p1, const void *p2);
// JAB: warning abatement
//static int lnk_fcmp (const void *p1, const void *p2);

void countdomains( char* coll, int32_t numRecs, int32_t verbosity, int32_t output ) {
	struct ip_info **ip_table;
	struct dom_info **dom_table;
	//HashTable ipHT;
	//HashTable domHT;
	//ipHT.set ( numRecs+1 );
	//domHT.set( numRecs+1 );

	CollectionRec *cr = g_collectiondb.getRec(coll);

	key_t startKey;
	key_t endKey  ;
	key_t lastKey ;
	startKey.setMin();
	endKey.setMax();
	lastKey.setMin();

	g_titledb.init ();
	//g_collectiondb.init(true);
	g_titledb.getRdb()->addRdbBase1(coll );

	log( LOG_INFO, "cntDm: parms: %s, %"INT32"", coll, numRecs );
	int64_t time_start = gettimeofdayInMilliseconds_force();

	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;
	Msg5 msg5;
	Msg5 msg5b;
	RdbList list;
	int32_t countDocs = 0;
	int32_t countIp = 0;
	int32_t countDom = 0;
	int32_t attempts = 0;

	ip_table  = (struct ip_info **)mmalloc(sizeof(struct ip_info *) * numRecs, 
					     "main-dcit" );
	dom_table = (struct dom_info **)mmalloc(sizeof(struct dom_info *) * numRecs,
					     "main-dcdt" );

	for( int32_t i = 0; i < numRecs; i++ ) {
		ip_table[i] = NULL;
		dom_table[i] = NULL;
	}
 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_TITLEDB   ,
			      cr->m_collnum       ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      true         , // Do we need to include tree?
			      false         , // add to cache?
			      0             , // max cache age
			      0             ,
			      -1            ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         , // err correction?
			      NULL          , // cache key ptr
			      0             , // retry num
			      -1            , // maxRetries
			      true          , // compensate for merge
			      -1LL          , // sync point
			      &msg5b        )){
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) goto freeInfo;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k       = list.getCurrentKey();
		char *rec     = list.getCurrentRec();
		int32_t  recSize = list.getCurrentRecSize();
		int64_t docId       = g_titledb.getDocId        ( &k );
		//int32_t      hostHash    = g_titledb.getHostHash     ( k );
		//int32_t      contentHash = g_titledb.getContentHash  ( k );
		attempts++;

		if ( k <= lastKey ) 
			log("key out of order. "
			    "lastKey.n1=%"XINT32" n0=%"XINT64" "
			    "currKey.n1=%"XINT32" n0=%"XINT64" ",
			    lastKey.n1,lastKey.n0,
			    k.n1,k.n0);
		lastKey = k;
		// print deletes
		if ( (k.n0 & 0x01) == 0) {
			fprintf(stderr,"n1=%08"XINT32" n0=%016"XINT64" docId=%012"INT64" "
				//"hh=%07"XINT32" ch=%08"XINT32" (del)\n", 
				"(del)\n", 
			       k.n1 , k.n0 , docId );
			continue;
		}

		if( (countIp >= numRecs) || (countDom >= numRecs) ) {
			log( LOG_INFO, "cntDm: countIp | countDom, greater than"
			     "numRecs requested, should never happen!!!!" );
			goto freeInfo;
		}

		// uncompress the title rec
		//TitleRec tr;
		//if ( ! tr.set ( rec , recSize , false ) )
		//	continue;
		XmlDoc xd;
		if ( ! xd.set2 (rec, recSize, coll,NULL,0) )
			continue;

		// extract the url
		//Url *u = tr.getUrl();
		
		struct ip_info  *sipi ;
		struct dom_info *sdomi;

		//uint32_t hkey_ip  = u->getIp();
		//uint32_t hkey_dom = hash32( u->getHost(), u->getHostLen() );
		//if( !(sipi = (struct ip_info *)ipHT.getValue( hkey_ip ))) {
		int32_t i;
		for( i = 0; i < countIp; i++ ) {
			if( !ip_table[i] ) continue;
			sipi = (struct ip_info *)ip_table[i];
			if( sipi->ip == (uint32_t)xd.m_ip ) break;
		}

		if( i == countIp ) {
			sipi = (struct ip_info *)mmalloc(sizeof(struct ip_info),
							 "main-dcip" );
			if( !sipi ) { char *XX=NULL; *XX=0; }
			//ipHT.addKey( hkey_ip, (int32_t)sipi, 0 );
			ip_table[countIp++]  = sipi;
			sipi->ip = xd.m_ip;//u->getIp();
			sipi->pages = 1;
			sipi->numDom = 0;
			//sipi->quality = tr.getDocQuality();
		}
		else { 
			sipi->pages++; 
			//sipi->quality += tr.getDocQuality(); 
		}
		
		//if( !(sdomi = (struct dom_info *)domHT.getValue( hkey_dom ))) {
		char *fu = xd.ptr_firstUrl;
		int32_t dlen; char *dom = getDomFast ( fu , &dlen );
		int32_t dkey = hash32( dom , dlen );

		for( i = 0; i < countDom; i++ ) {
			if( !dom_table[i] ) continue;
			sdomi = (struct dom_info *)dom_table[i];
			/*
			int32_t len = u->getHostLen();
			if( sdomi->domLen < u->getHostLen() ) len=sdomi->domLen;
			if(strncasecmp(sdomi->dom, u->getHost(), len)==0) break;
			*/
			if( sdomi->dHash == dkey ) break;
		}

		if( i == countDom ) {
			sdomi =(struct dom_info*)mmalloc(sizeof(struct dom_info),
							 "main-dcdm" );
			if( !sdomi ) { char *XX=NULL; *XX=0; }
			//domHT.addKey( hkey_dom, (int32_t)sdomi, 0 );
			dom_table[countDom++] = sdomi;
			sdomi->dom = (char *)mmalloc( dlen,"main-dcsdm" );

			strncpy( sdomi->dom, dom , dlen );
			sdomi->domLen = dlen;
			sdomi->dHash = dkey;
			sdomi->pages = 1;
			//sdomi->quality = tr.getDocQuality();
			sdomi->numIp = 0;

			//sdomi->dht = new( HashTable );
			//mnew( sdomi->dht, sizeof(HashTable), "main-dcndht" );
			//sdomi->dht->set( 1000 );
			sdomi->tableSize = 0;
			sdomi->lnkCnt = 0;
		}
		else { 
			sdomi->pages++; 
			//sdomi->quality += tr.getDocQuality(); 
		}

		Links *dlinks = xd.getLinks();

		/*
		// Parse outgoing links and count frequency
		Links dLinks;
		//Xml *sx;
		//sx  = g_tagdb.getSiteXml ( tr.getSiteFilenum(), coll , 
		//			    gbstrlen( coll ) );
		Xml xml;
		if (!xml.set( tr.getCharset(), tr.getContent(), tr.getContentLen(),
			      false, 0, false, tr.getVersion() )){
			log(LOG_WARN, "countdomains: error setting Xml: %s", 
			    mstrerror(g_errno));
			return;
		}
			
		if (!dLinks.set( true, &xml, tr.getUrl(), false, false, 
				 xd.m_version,0 )){
			log(LOG_WARN, "countdomains: error setting Links: %s",
			    mstrerror(g_errno));
			return;
		}
		*/
		int32_t size = dlinks->getNumLinks();
		if( !sdomi->tableSize ) {
			sdomi->lnk_table = (int32_t *)mmalloc(size * sizeof(int32_t),
							   "main-dclt" );
			sdomi->tableSize = size;
		}
		else {
			if( size > (sdomi->tableSize - sdomi->lnkCnt) ) {
				size += sdomi->lnkCnt;
				sdomi->lnk_table = (int32_t *)
					mrealloc(sdomi->lnk_table,
						 sdomi->tableSize*sizeof(int32_t),
						 size*sizeof(int32_t),
						 "main-dcrlt" );
				sdomi->tableSize = size;
			}
		}
			
		for( int32_t i = 0; i < dlinks->getNumLinks(); i++ ) {
			//struct lnk_info *slink;
			//Url url;
			//url.set(dLinks.getLink(i), dLinks.getLinkLen(i));
			char *link = dlinks->getLink(i);
			int32_t dlen; char *dom = getDomFast ( link , &dlen );
			uint32_t lkey = hash32( dom , dlen );
			//if( (slink = (struct lnk_info *)
			//                sdomi->dht->getValue( lkey ))) {
			int32_t j;
			for( j = 0; j < sdomi->lnkCnt; j++ ) {
				//slink=(struct lnk_info *)sdomi->lnk_table[j];
				if( sdomi->lnk_table[j] == (int32_t)lkey ) break;
				//if(slink->domLen != url.getHostLen()) continue;
				//if( !strcasecmp( slink->dom, url.getHost() ) ) 
				//break;
			}
			
			sdomi->lnkPages++;
			if( j != sdomi->lnkCnt ) continue;
			sdomi->lnk_table[sdomi->lnkCnt++] = lkey;
			sdomi->lnkPages++;
			//slink=(struct lnk_info *)mmalloc(sizeof(struct lnk_info),
			//				 "main-dcli" );
			//Sanity check, mallocing link_info struct
			//if( !slink ) { char *XX=NULL; *XX=0; }
			//sdomi->dht->addKey( lkey, (int32_t)slink, 0 );
			//sdomi->lnk_table[sdomi->lnkCnt++] = (int32_t)slink;
			//slink->dom = (char *)mmalloc( url.getHostLen(),
			//			      "main-dcsld" );
			//strncpy( slink->dom, url.getHost(),
			//	 url.getHostLen() );
			//slink->domLen = url.getHostLen();
			//slink->pages = 1;
		}

		// Handle lists
		if( !sipi->numDom || !sdomi->numIp ){
			sdomi->numIp++; sipi->numDom++;
			//Add to IP list for Domain
			sdomi->ip_list = (struct ip_info **)
				mrealloc( sdomi->ip_list,
					  (sdomi->numIp-1)*sizeof(char *),
					  sdomi->numIp*sizeof(char *),
					  "main-dcldm" );
			sdomi->ip_list[sdomi->numIp-1] = sipi;

			//Add to domain list for IP
			sipi->dom_list = (struct dom_info **)
				mrealloc( sipi->dom_list,
					  (sipi->numDom-1)*sizeof(char *),
					  sipi->numDom*sizeof(char *),
					  "main-dclip" );
			sipi->dom_list[sipi->numDom-1] = sdomi;
		}
		else {
			int32_t i;
			for( i = 0; 
			     (i < sdomi->numIp) 
				     && (sdomi->ip_list[i] != sipi);
			     i++ );
			if( sdomi->numIp != i ) goto updateIp;

			sdomi->numIp++;
			sdomi->ip_list = (struct ip_info **)
				mrealloc( sdomi->ip_list,
					  (sdomi->numIp-1)*sizeof(int32_t),
					  sdomi->numIp*sizeof(int32_t),
					  "main-dcldm" );
			sdomi->ip_list[sdomi->numIp-1] = sipi;

		updateIp:
			for( i = 0; 
			     (i < sipi->numDom) 
				     && (sipi->dom_list[i] != sdomi);
			     i++ );
			if( sipi->numDom != i ) goto endListUpdate;

			sipi->numDom++;
			sipi->dom_list = (struct dom_info **)
				mrealloc( sipi->dom_list,
					  (sipi->numDom-1)*sizeof(int32_t),
					  sipi->numDom*sizeof(int32_t),
					  "main-dclip" );
			sipi->dom_list[sipi->numDom-1] = sdomi;

		endListUpdate:
			i=0;
		}				
		if( !((++countDocs) % 1000) ) 
			log(LOG_INFO, "cntDm: %"INT32" records searched.",countDocs);
		if( countDocs == numRecs ) goto freeInfo;
		//else countDocs++;
	}
	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around
	if ( startKey < *(key_t *)list.getLastKey() ) {
		log( LOG_INFO, "cntDm: Keys wrapped around! Exiting." );
		goto freeInfo;
	}
		
	if ( countDocs >= numRecs ) {
	freeInfo:
		char             buf[128];
		//int32_t             value   ;
		int32_t             len     ;
		char             loop    ;
		int32_t             recsDisp;
		struct ip_info  *tmpipi  ;
		struct dom_info *tmpdomi ;
		//struct lnk_info *tmplnk  ;
		loop = 0;

		FILE *fhndl;		
		char out[128];
		if( output != 9 ) goto printHtml;
		// Dump raw data to a file to parse later
		sprintf( out, "%scntdom.xml", g_hostdb.m_dir );
		if( (fhndl = fopen( out, "wb" )) < 0 ) {
			log( LOG_INFO, "cntDm: File Open Failed." );
			return;
		}

		gbsort( dom_table, countDom, sizeof(struct dom_info *), dom_fcmp );		
		for( int32_t i = 0; i < countDom; i++ ) {
			if( !dom_table[i] ) continue;
			tmpdomi = (struct dom_info *)dom_table[i];
			len = tmpdomi->domLen;
			if( tmpdomi->domLen > 127 ) len = 126;
			strncpy( buf, tmpdomi->dom, len );
			buf[len] = '\0';
			fprintf(fhndl,
				"<rec1>\n\t<domain>%s</domain>\n"
				"\t<pages>%"INT32"</pages>\n"
				//"\t<quality>%"INT64"</quality>\n"
				"\t<block>\n",
				buf, tmpdomi->pages
				//,(tmpdomi->quality/tmpdomi->pages)
				);
			gbsort( tmpdomi->ip_list,tmpdomi->numIp, sizeof(int32_t), 
			       ip_fcmp );
			for( int32_t j = 0; j < tmpdomi->numIp; j++ ) {
				if( !tmpdomi->ip_list[j] ) continue;
				tmpipi = (struct ip_info *)tmpdomi->ip_list[j];
				strcpy ( buf , iptoa( tmpipi->ip ) );
				fprintf(fhndl,"\t\t<ip>%s</ip>\n",buf);
			}
			fprintf(fhndl,
				"\t</block>\n"
				"\t<links>\n");
			/*
			gbsort(tmpdomi->lnk_table,tmpdomi->lnkCnt,sizeof(int32_t), 
			       lnk_fcmp );
			for( int32_t j = 0; j < tmpdomi->lnkCnt; j++ ) {
				tmplnk = (struct lnk_info *)tmpdomi->lnk_table[j];
				len = tmplnk->domLen;
				if( len > 127 ) len = 126;
				strncpy( buf, tmplnk->dom, len );
				buf[len] = '\0';
				fprintf(fhndl,
					"\t\t<link>\n"
					"\t\t\t<domain>%s</domain>\n"
					"\t\t\t<pages>%"INT32"</pages>\n"
					"\t\t</link>\n",
					buf, tmplnk->pages);
			}
			fprintf(fhndl,
				"\t</links>\n"
				"</rec1>\n");
			*/
		}
		gbsort( ip_table, countIp, sizeof(struct ip_info *), ip_fcmp );		
		for( int32_t i = 0; i < countIp; i++ ) {
			if( !ip_table[i] ) continue;
			tmpipi = (struct ip_info *)ip_table[i];
			strcpy ( buf , iptoa( tmpipi->ip ) );
			fprintf(fhndl,
				"<rec2>\n\t<ip>%s</ip>\n"
				"\t<pages>%"INT32"</pages>\n"
				//"\t<quality>%"INT64"</quality>\n"
				"\t<block>\n",
				buf, tmpipi->pages);
			//(tmpipi->quality/tmpipi->pages));
			for( int32_t j = 0; j < tmpipi->numDom; j++ ) {
				tmpdomi = (struct dom_info *)tmpipi->dom_list[j];
				len = tmpdomi->domLen;
				if( tmpdomi->domLen > 127 ) len = 126;
				strncpy( buf, tmpdomi->dom, len );
				buf[len] = '\0';
				fprintf(fhndl,
					"\t\t<domain>%s</domain>\n",
					buf);
			}
			fprintf(fhndl,
				"\t</block>\n"
				"</rec2>\n");
		}

		if( fclose( fhndl ) < 0 ) {
			log( LOG_INFO, "cntDm: File Close Failed." );
			return;
		}
		fhndl = 0;

		/*
		// Terminal Output format
		for( int32_t i = 0; i < countIp; i++ ) {
			if( !ip_table[i] ) continue;
			tmpipi = (struct ip_info *)ip_table[i];
			strcpy ( buf , iptoa( tmpipi->ip ) );
			fprintf( stderr, "\t\tIP: %s   \t\t\t\t\t%"INT32"\n", buf, 
				 tmpipi->pages );
			for( int32_t j = 0; j < tmpipi->numDom; j++ ) {
				int32_t len;
				tmpdomi = (struct dom_info *)tmpipi->dom_list[j];
				len = tmpdomi->domLen;
				if( tmpdomi->domLen > 127 ) len = 126;
				strncpy( buf, tmpdomi->dom, len );
				buf[len] = '\0';
				fprintf( stderr, "\t\t\tDM: %s",
					 buf );
				if( tmpdomi->domLen > 27 )
					fprintf( stderr, "\t\t" );
				else if( tmpdomi->domLen <= 11 )
					fprintf( stderr, "\t\t\t\t\t" );
				else if( tmpdomi->domLen >= 20 )
					fprintf( stderr, "\t\t\t" );
				else
					fprintf( stderr, "\t\t\t\t" );
				fprintf( stderr, "%"INT32"\n", tmpdomi->pages );

				if( verbosity != 10 ) continue;
				gbsort( tmpdomi->lnk_table, tmpdomi->lnkCnt, 
				       sizeof(int32_t), lnk_fcmp );
				for( int32_t k = 0; k < tmpdomi->lnkCnt; k++ ) {
					tmplnk = (struct lnk_info *)
						tmpdomi->lnk_table[k];
					len = tmplnk->domLen;
					if( len > 127 ) len = 126;
					strncpy( buf, tmplnk->dom, len );
					buf[len] = '\0';
					fprintf( stderr, "\t\t\t\tLD: %s",
						 buf );
					if( len > 27 )
						fprintf( stderr, "\t" );
					else if( len <= 11 )
						fprintf( stderr, "\t\t\t\t" );
					else if( len >= 20 )
						fprintf( stderr, "\t\t" );
					else
						fprintf( stderr, "\t\t\t" );
					fprintf(stderr, "%"INT32"\n", 
						tmplnk->pages);
				}
				
			}
			fprintf( stderr, "\n" );
		}
		*/
	printHtml:
		// HTML file Output
		sprintf( out, "%scntdom.html", g_hostdb.m_dir );
		if( (fhndl = fopen( out, "wb" )) < 0 ) {
			log( LOG_INFO, "cntDm: File Open Failed." );
			return;
		}		
		int64_t total = g_titledb.getGlobalNumDocs();
		char link_ip[]  = "http://www.gigablast.com/search?"
			          "code=gbmonitor&q=ip%3A";
		char link_dom[] = "http://www.gigablast.com/search?"
			          "code=gbmonitor&q=site%3A";
		char menu[] = "<table cellpadding=\"2\" cellspacing=\"2\">\n<tr>"
			"<th bgcolor=\"#CCCC66\"><a href=\"#pid\">"
			"Domains Sorted By Pages</a></th>"
			"<th bgcolor=\"#CCCC66\"><a href=\"#lid\">"
			"Domains Sorted By Links</a></th>"
			"<th bgcolor=\"#CCCC66\"><a href=\"#pii\">"
			"IPs Sorted By Pages</a></th>"
			"<th bgcolor=\"#CCCC66\"><a href=\"#dii\">"
			"IPs Sorted By Domains</a></th>"
			"<th bgcolor=\"#CCCC66\"><a href=\"#stats\">"
			"Stats</a></th>"
			"</tr>\n</table>\n<br>\n";

		char hdr[] = "<table cellpadding=\"5\" cellspacing=\"2\">"
			"<tr bgcolor=\"AAAAAA\">"
			"<th>Domain</th>"
			"<th>Domains Linked</th>"
			//"<th>Avg Quality</th>"
			"<th># Pages</th>"
			"<th>Extrap # Pages</th>"
			"<th>IP</th>"
			"</tr>\n";

		char hdr2[] = "<table cellpadding=\"5\" cellspacing=\"2\">"
			"<tr bgcolor=\"AAAAAA\">"
			"<th>IP</th>"
			"<th>Domain</th>"
			"<th>Domains Linked</th>"
			//"<th>Avg Quality</th>"
			"<th># Pages</th>"
			"<th>Extrap # Pages</th>"
			"</tr>\n";
		
		char clr1[] = "#FFFF00";//"yellow";
		char clr2[] = "#FFFF66";//"orange";
		//char clr3[] = "#0099FF";//"#66FF33";
		//char clr4[] = "#33FFCC";//"#33CC33";
		char *color;
			
		fprintf( fhndl, 
			 "<html><head><title>Domain/IP Counter</title></head>\n"
			 "<body>"
			 "<h1>Domain/IP Counter</h1><br><br>"
			 "<a name=\"stats\">"
			 "<h2>Stats</h2>\n%s", menu );

		// Stats
		fprintf( fhndl, "<br>\n\n<table>\n"
			 "<tr><th align=\"left\">Total Number of Domains</th>"
			 "<td>%"INT32"</td></tr>\n"
			 "<tr><th align=\"left\">Total Number of Ips</th>"
			 "<td>%"INT32"</td></tr>\n"
			 "<tr><th align=\"left\">Number of Documents Searched"
			 "</th><td>%"INT32"</td></tr>\n"
			 "<tr><th align=\"left\">Number of Failed Attempts</th>"
			 "<td>%"INT32"</td></tr><tr></tr><tr>\n"
			 "<tr><th align=\"left\">Number of Documents in Index"
			 "</th><td>%"INT64"</td></tr>\n"
			 "<tr><th align=\"left\">Estimated Domains in index</th>"
			 "<td>%"INT64"</td></tr>"
			 "</table><br><br><br>\n"
			 ,countDom,countIp,
			 countDocs, attempts-countDocs,total, 
			 ((countDom*total)/countDocs) );
		
		
		fprintf( fhndl, "<a name=\"pid\">\n"
			 "<h2>Domains Sorted By Pages</h2>\n"
			 "%s", menu );
		gbsort( dom_table, countDom, sizeof(struct dom_info *), dom_fcmp );
	printDomLp:

		fprintf( fhndl,"%s", hdr );
		recsDisp = countDom;
		if( countDom > 1000 ) recsDisp = 1000;
		for( int32_t i = 0; i < recsDisp; i++ ) {
			char buf[128];
			int32_t len;
			if( !dom_table[i] ) continue;
			if( i%2 ) color = clr2;
			else color = clr1;
			tmpdomi = (struct dom_info *)dom_table[i];
			len = tmpdomi->domLen;
			if( tmpdomi->domLen > 127 ) len = 126;
			strncpy( buf, tmpdomi->dom, len );
			buf[len] = '\0';
			fprintf( fhndl, "<tr bgcolor=\"%s\"><td>"
				 "<a href=\"%s%s\" target=\"_blank\">%s</a>"
				 "</td><td>%"INT32"</td>"
				 //"<td>%"INT64"</td>"
				 "<td>%"INT32"</td>"
				 "<td>%"INT64"</td><td>", 
				 color, link_dom,
				 buf, buf, tmpdomi->lnkCnt,
				 //(tmpdomi->quality/tmpdomi->pages), 
				 tmpdomi->pages,
				 ((tmpdomi->pages*total)/countDocs) );
			for( int32_t j = 0; j < tmpdomi->numIp; j++ ) {
				tmpipi = (struct ip_info *)tmpdomi->ip_list[j];
				strcpy ( buf , iptoa(tmpipi->ip) );
				fprintf( fhndl, "<a href=\"%s%s\""
					 "target=\"_blank\">%s</a>\n", 
					 link_ip, buf, buf );
			}
			fprintf( fhndl, "</td></tr>\n" );
			/*
			if( verbosity != 10 ) goto printDone; 
			gbsort(tmpdomi->lnk_table,tmpdomi->lnkCnt,sizeof(int32_t), 
			       lnk_fcmp );
			for( int32_t k = 0; k < tmpdomi->lnkCnt; k++ ) {
				tmplnk = (struct lnk_info *)tmpdomi->lnk_table[k];
				len = tmplnk->domLen;
				if( len > 127 ) len = 126;
				strncpy( buf, tmplnk->dom, len );
				buf[len] = '\0';
				fprintf( fhndl, "\t\t<tr bgcolor=\"green\"><td>"
					 "</td><td></td><td>%s</td><td></td><td>"
					 "%"INT32"</td><td>%"INT64"</td></tr>\n", buf, 
					 tmplnk->pages,
					 ((tmplnk->pages*total)/countDocs) );
			}
		printDone:
			*/
			fprintf( fhndl, "\n" );
		}

		fprintf( fhndl, "</table>\n<br><br><br>" );
		if( loop == 0 ) {
			loop = 1;
			gbsort( dom_table, countDom, sizeof(struct dom_info *), dom_lcmp );
			fprintf( fhndl, "<a name=\"lid\">"
				 "<h2>Domains Sorted By Links</h2>\n%s", menu );

			goto printDomLp;
		}
		loop = 0;

		fprintf( fhndl, "<a name=\"pii\">"
			 "<h2>IPs Sorted By Pages</h2>\n%s", menu );


		gbsort( ip_table, countIp, sizeof(struct ip_info *), ip_fcmp );
	printIpLp:
		fprintf( fhndl,"%s", hdr2 );
		recsDisp = countIp;
		if( countIp > 1000 ) recsDisp = 1000;
		for( int32_t i = 0; i < recsDisp; i++ ) {
			char buf[128];
			if( !ip_table[i] ) continue;
			tmpipi = (struct ip_info *)ip_table[i];
			strcpy ( buf , iptoa(tmpipi->ip) );
			if( i%2 ) color = clr2;
			else color = clr1;
			int32_t linked = 0;
			for( int32_t j = 0; j < tmpipi->numDom; j++ ) {
				tmpdomi=(struct dom_info *)tmpipi->dom_list[j];
				linked += tmpdomi->lnkCnt;
			}
			fprintf( fhndl, "\t<tr bgcolor=\"%s\"><td>"
				 "<a href=\"%s%s\" target=\"_blank\">%s</a>"
				 "</td>"
				 "<td>%"INT32"</td>"
				 "<td>%"INT32"</td>"
				 //"<td>%"INT64"</td>"
				 "<td>%"INT32"</td>"
				 "<td>%"INT64"</td></tr>\n", 
				 color,
				 link_ip, buf, buf, tmpipi->numDom, linked,
				 //(tmpipi->quality/tmpipi->pages), 
				 tmpipi->pages, 
				 ((tmpipi->pages*total)/countDocs) );
			/*
			for( int32_t j = 0; j < tmpipi->numDom; j++ ) {
			        int32_t len;
				tmpdomi=(struct dom_info *)tmpipi->dom_list[j];
				len = tmpdomi->domLen;
				if( tmpdomi->domLen > 127 ) len = 126;
				strncpy( buf, tmpdomi->dom, len );
				buf[len] = '\0';
				if( j%2 ) color = clr4;
				else color = clr3;
				fprintf( fhndl, "<tr bgcolor=\"%s\"><td>"
					 "</td><td><a href=\"%s%s\">%s</a></td>"
					 "<td>%"INT32"</td><td>%"INT64""
					 "</td><td>%"INT32"</td><td> %"INT64"</td></tr>"
					 "\n", color, link_dom, buf,
					 buf, tmpdomi->lnkCnt,
					 (tmpdomi->quality/tmpdomi->pages), 
					 tmpdomi->pages, 
					 ((tmpdomi->pages*total)/countDocs) );
			}
			*/
			fprintf( fhndl, "\n" );
		}

		fprintf( fhndl, "</table>\n<br><br><br>" );
		if( loop == 0 ) {
			loop = 1;
			gbsort( ip_table, countIp, sizeof(struct ip_table *), ip_dcmp );
			fprintf( fhndl, "<a name=\"dii\">"
				 "<h2>IPs Sorted By Domains</h2>\n%s", menu );
			goto printIpLp;
		}

		if( fclose( fhndl ) < 0 ) {
			log( LOG_INFO, "cntDm: File Close Failed." );
			return;
		}
		fhndl = 0;


		int32_t ima = 0;
		int32_t dma = 0;

		log( LOG_INFO, "cntDm: Freeing ip info struct..." );
		for( int32_t i = 0; i < countIp; i++ ) {
			if( !ip_table[i] ) continue;
			//value = ipHT.getValue( ip_table[i] );
			//if(value == 0) continue;
			tmpipi = (struct ip_info *)ip_table[i]; 
			mfree( tmpipi->dom_list, tmpipi->numDom*sizeof(int32_t),
			       "main-dcflip" );
			ima += tmpipi->numDom * sizeof(int32_t);
			mfree( tmpipi, sizeof(struct ip_info), "main-dcfip" );
			ima += sizeof(struct ip_info);
			tmpipi = NULL;
		}
		mfree( ip_table, numRecs * sizeof(struct ip_table *), "main-dcfit" );

		log( LOG_INFO, "cntDm: Freeing domain info struct..." );
		for( int32_t i = 0; i < countDom; i++ ) {
			if( !dom_table[i] ) continue;
			tmpdomi = (struct dom_info *)dom_table[i];
			/*
			for( int32_t j = 0; j < tmpdomi->lnkCnt; j++ ) {
				if( !tmpdomi->lnk_table[j] ) continue;
				tmplnk=(struct lnk_info *)tmpdomi->lnk_table[j];
				mfree( tmplnk->dom, tmplnk->domLen, 
				       "main-dsfsld" );
				mfree( tmplnk, sizeof(struct lnk_info),
				       "main-dsfsli" );
			}
			*/
			mfree( tmpdomi->lnk_table, 
			       tmpdomi->tableSize*sizeof(int32_t), 
			       "main-dcfsdlt" );
			dma += tmpdomi->tableSize * sizeof(int32_t);
			mfree( tmpdomi->ip_list, tmpdomi->numIp*sizeof(int32_t),
			       "main-dcfldom" );
			dma += tmpdomi->numIp * sizeof(int32_t);
			mfree( tmpdomi->dom, tmpdomi->domLen, "main-dcfsdom" );
			dma += tmpdomi->domLen;
			//tmpdomi->dht.reset();
			//mdelete( tmpdomi->dht, sizeof(HashTable), "main-dcmdht" );
			//delete tmpdomi->dht;
			mfree( tmpdomi, sizeof(struct dom_info), "main-dcfdom" );
			dma+= sizeof(struct dom_info);
			tmpdomi = NULL;
		}
					
		mfree( dom_table, numRecs * sizeof(struct dom_info *), "main-dcfdt" );

		int64_t time_end = gettimeofdayInMilliseconds_force();
		log( LOG_INFO, "cntDm: Took %"INT64"ms to count domains in %"INT32" recs.",
		     time_end-time_start, countDocs );
		log( LOG_INFO, "cntDm: %"INT32" bytes of Total Memory Used.", 
		     ima + dma + (8 * numRecs) );
		log( LOG_INFO, "cntDm: %"INT32" bytes Total for IP.", ima );
		log( LOG_INFO, "cntDm: %"INT32" bytes Total for Dom.", dma );
		log( LOG_INFO, "cntDm: %"INT32" bytes Average for IP.", ima/countIp );
		log( LOG_INFO, "cntDm: %"INT32" bytes Average for Dom.", dma/countDom );
		
		return;
	}	
	goto loop;	
}

// Sort by IP frequency in pages 9->0
int ip_fcmp (const void *p1, const void *p2) {
	//int32_t n1, n2;
	// break this! need to fix later MDW 11/12/14
	char *n1 ;
	char *n2 ;
	struct ip_info *ii1;
	struct ip_info *ii2;

	*(((unsigned char *)(&n1))+0) = *(((char *)p1)+0);
	*(((unsigned char *)(&n1))+1) = *(((char *)p1)+1);
	*(((unsigned char *)(&n1))+2) = *(((char *)p1)+2);
	*(((unsigned char *)(&n1))+3) = *(((char *)p1)+3);

	*(((unsigned char *)(&n2))+0) = *(((char *)p2)+0);
	*(((unsigned char *)(&n2))+1) = *(((char *)p2)+1);
	*(((unsigned char *)(&n2))+2) = *(((char *)p2)+2);
	*(((unsigned char *)(&n2))+3) = *(((char *)p2)+3);

	ii1 = (struct ip_info *)n1;
	ii2 = (struct ip_info *)n2;
	
	return ii2->pages-ii1->pages;
}

// Sort by number of domains linked to IP, descending
int ip_dcmp (const void *p1, const void *p2) {
	//int32_t n1, n2;
	// break this! need to fix later MDW 11/12/14
	char *n1 ;
	char *n2 ;

	struct ip_info *ii1;
	struct ip_info *ii2;

	*(((unsigned char *)(&n1))+0) = *(((char *)p1)+0);
	*(((unsigned char *)(&n1))+1) = *(((char *)p1)+1);
	*(((unsigned char *)(&n1))+2) = *(((char *)p1)+2);
	*(((unsigned char *)(&n1))+3) = *(((char *)p1)+3);

	*(((unsigned char *)(&n2))+0) = *(((char *)p2)+0);
	*(((unsigned char *)(&n2))+1) = *(((char *)p2)+1);
	*(((unsigned char *)(&n2))+2) = *(((char *)p2)+2);
	*(((unsigned char *)(&n2))+3) = *(((char *)p2)+3);

	ii1 = (struct ip_info *)n1;
	ii2 = (struct ip_info *)n2;
	
	return ii2->numDom-ii1->numDom;
}

// Sort by page frequency in titlerec 9->0
int dom_fcmp (const void *p1, const void *p2) {
	//int32_t n1, n2;
	// break this! need to fix later MDW 11/12/14
	char *n1 ;
	char *n2 ;
	struct dom_info *di1;
	struct dom_info *di2;

	*(((unsigned char *)(&n1))+0) = *(((char *)p1)+0);
	*(((unsigned char *)(&n1))+1) = *(((char *)p1)+1);
	*(((unsigned char *)(&n1))+2) = *(((char *)p1)+2);
	*(((unsigned char *)(&n1))+3) = *(((char *)p1)+3);

	*(((unsigned char *)(&n2))+0) = *(((char *)p2)+0);
	*(((unsigned char *)(&n2))+1) = *(((char *)p2)+1);
	*(((unsigned char *)(&n2))+2) = *(((char *)p2)+2);
	*(((unsigned char *)(&n2))+3) = *(((char *)p2)+3);


	di1 = (struct dom_info *)n1;
	di2 = (struct dom_info *)n2;

	return di2->pages-di1->pages;
}

// Sort by quantity of outgoing links 9-0
int dom_lcmp (const void *p1, const void *p2) {
	//int32_t n1, n2;
	// break this! need to fix later MDW 11/12/14
	char *n1 ;
	char *n2 ;
	struct dom_info *di1;
	struct dom_info *di2;

	*(((unsigned char *)(&n1))+0) = *(((char *)p1)+0);
	*(((unsigned char *)(&n1))+1) = *(((char *)p1)+1);
	*(((unsigned char *)(&n1))+2) = *(((char *)p1)+2);
	*(((unsigned char *)(&n1))+3) = *(((char *)p1)+3);

	*(((unsigned char *)(&n2))+0) = *(((char *)p2)+0);
	*(((unsigned char *)(&n2))+1) = *(((char *)p2)+1);
	*(((unsigned char *)(&n2))+2) = *(((char *)p2)+2);
	*(((unsigned char *)(&n2))+3) = *(((char *)p2)+3);


	di1 = (struct dom_info *)n1;
	di2 = (struct dom_info *)n2;

	return di2->lnkCnt-di1->lnkCnt;
}

// JAB: warning abatement
#if 0
// Sort by frequency of link use, 9-0
int lnk_fcmp (const void *p1, const void *p2) {
	//int32_t n1, n2;
	// break this! need to fix later MDW 11/12/14
	char *n1 ;
	char *n2 ;
	struct lnk_info *li1;
	struct lnk_info *li2;

	*(((unsigned char *)(&n1))+0) = *(((char *)p1)+0);
	*(((unsigned char *)(&n1))+1) = *(((char *)p1)+1);
	*(((unsigned char *)(&n1))+2) = *(((char *)p1)+2);
	*(((unsigned char *)(&n1))+3) = *(((char *)p1)+3);

	*(((unsigned char *)(&n2))+0) = *(((char *)p2)+0);
	*(((unsigned char *)(&n2))+1) = *(((char *)p2)+1);
	*(((unsigned char *)(&n2))+2) = *(((char *)p2)+2);
	*(((unsigned char *)(&n2))+3) = *(((char *)p2)+3);


	li1 = (struct lnk_info *)n1;
	li2 = (struct lnk_info *)n2;

	return li2->pages-li1->pages;
}
#endif

/*
static void printBits(qvec_t bits, int32_t numDigits, char *buf){
	int32_t pos = 0;
	for (int32_t i=0; i < numDigits ; i++){
		if (i && i%4 == 0) buf[pos++] = ' ';
		if (bits & (1 << (numDigits-i-1))) buf[pos++] = '1';
		else buf[pos++] = '0';
	}
	buf[pos] = 0;
}

bool testBoolean() {
	if (!queryTest()) return false;

	char *testQueries [] = {
		"a AND b OR c",
		"a OR b AND c",
		"a AND NOT b OR b AND NOT a",
		//vivismo query bug
		"canada suntanning OR beaches",
		"canada AND suntanning OR beaches",
		"canada AND (suntanning OR beaches)",
		"(canada AND suntanning) OR beaches",
		"a AND b OR c AND d AND e OR f",
		// buzz problem query
		"(a AND NOT (b OR c)) d | f",
		"foo AND (bar OR boo)  keywords | sortkey"

// 		"a AND NOT b OR c",
// 		"a AND NOT b OR b AND NOT a",
// 		"a OR b | c",
// 		"(a AND b OR c) | d",
	};
	char *truthTables [] = {
		"00011111",
		"01010111",
		"0110",
		// term 0 has implicitbits for 1 and 2
		"0101011111111111", 
		"00011111",
		"00010101",
		"00011111",
		// big uns
		"00010001000100010001000100011111"
		"11111111111111111111111111111111",

		"00000000000000000000000001000000",

		"00000000000000000000000000010101",
	};
	int numTests = 10;
	// buffer for holding truth table
	int32_t bufSize = 10000000;
	char *bitScoresBuf = (char*) mmalloc(bufSize, "bitScoreBuf");
	if (!bitScoresBuf){
		log("query: unable to alloc bitScores buffer: %s",
		    mstrerror(g_errno) );
		return false; 
	}
	for (int i=0; i < numTests ; i++) {
		Query q;
		if ( ! q.set2 ( testQueries[i] , langUnknown ) ) {
			log("query: unable to set query: %s",
			    mstrerror(g_errno) );
			continue;
		}

		q.setBitMap();
		if ( ! q.setBitScoresBoolean(bitScoresBuf, bufSize) ) {
			log("query: unable to set bitScores: %s",
			    mstrerror(g_errno) );
			mfree(bitScoresBuf, bufSize,"bitScoresBuf");
			return false; 
		}
		 
		printf("\n");
		log(LOG_INIT, "query: Test #%d: %s",
		    i, testQueries[i]);

		// print parsed expressions
		SafeBuf sbuf(1024);
		Expression *e = &q.m_expressions[0];
		while (e->m_parent) e = e->m_parent;
		e->print(&sbuf);

		log("query: %s", sbuf.getBufStart());
		int32_t numCombos = 1 << q.m_numExplicitBits;
		//log("query: numcombos: %d", numCombos);

		// hack for duplicate terms bits so we don't need 
		// an unreasonably
		// large test table
		qvec_t bitMask = 0;
		for (int j=0;j<q.m_numTerms;j++){
			QueryTerm *qt = &q.m_qterms[j];
			bitMask |= qt->m_explicitBit;
			bitMask |= qt->m_implicitBits;
			sbuf.reset();
			//sbuf.utf16Encode(qt->m_term, qt->m_termLen);
			sbuf.safeMemcpy(qt->m_term, qt->m_termLen);
			log("query: term #%d: ebit=0x08%"XINT64" ibit=0x08%"XINT64" %s", 
			    j, 
			    (int64_t) q.m_qterms[j].m_explicitBit,
			    (int64_t) q.m_qterms[j].m_implicitBits,
			    sbuf.getBufStart());
		}
		//some problem queries give no terms, and a zero bitmask 
		// causes it to produce no errors
		if (!bitMask) bitMask = numCombos-1;

		int32_t errorCount = 0;
		char bitBuf[64];
		bitBuf[63] = 0;
		printBits(bitMask, q.m_numExplicitBits, bitBuf);
		
		log("query: bit mask: 0x%08llx (%s)", 
		    (int64_t) bitMask, bitBuf);
		for (int j=0;j<numCombos;j++){
			qvec_t bits = j & bitMask;
			char ttval = truthTables[i][bits]-'0';
			// sanity check...if we go over bounds of truthTable 
			// array, we are in the test query array and
			// weird stuff happens
			if (ttval != 0 && ttval != 1){
				log("query: error in truth table #%d!!!",i);
				char *xx=NULL;*xx=0;
			}

			printBits(bits,q.m_numExplicitBits,bitBuf);
			if (q.m_bitScores[bits]) 
				log(LOG_INIT, "query: 0x%04llx: (%s) true", 
				    (int64_t) bits, bitBuf);

			if (q.m_bitScores[bits] && ttval)
				continue;
			if (!q.m_bitScores[bits] && !ttval) 
				continue;
			errorCount++;
			printBits(bits, q.m_numExplicitBits, bitBuf);
			log("query: ERROR! 0x%04llx: %s %s",
			    (int64_t)bits, bitBuf, 
			    q.m_bitScores[bits]?"true":"false");
		}
		if (!errorCount) log(LOG_INIT,
				     "query: Test #%d Passed (%"INT32" values)", 
				     i, numCombos);
		else log(LOG_WARN, "Test #%d FAILED %"INT32" of %"INT32" truth values", 
			 i, errorCount, numCombos);
		
	}
	mfree(bitScoresBuf, bufSize,"bitScoresBuf");
	return true;
}
*/

//#include "LinkText.h"

/*
void testSpamRules(char *coll,
		   int32_t startFileNum,
		   int32_t numFiles,
		   bool includeTree,
		   int64_t docid) {
	//int32_t collLen = gbstrlen(coll);
	//g_conf.m_spiderdbMaxTreeMem = 1024*1024*30;
	g_conf.m_spiderdbMaxDiskPageCacheMem   = 0;
	g_titledb.init ();
	g_collectiondb.init(true);
	g_titledb.getRdb()->addRdbBase1 ( coll );
	key_t startKey ;
	key_t endKey   ;
	key_t lastKey  ;
	startKey.setMin();
	endKey.setMax();
	lastKey.setMin();
	startKey = g_titledb.makeFirstTitleRecKey ( docid );
	// turn off threads
	g_threads.disableThreads();
	// get a meg at a time
	int32_t minRecSizes = 1024*1024;
	Msg5 msg5;
	Msg5 msg5b;
	Msg5 msg5c;
	RdbList list;
	RdbList ulist;

	if (!ucInit(g_hostdb.m_dir, true)) {
		log("Unicode initialization failed!");
	}

	g_collectiondb.init(true);

 loop:
	// use msg5 to get the list, should ALWAYS block since no threads
	if ( ! msg5.getList ( RDB_TITLEDB   ,
			      coll          ,
			      &list         ,
			      startKey      ,
			      endKey        ,
			      minRecSizes   ,
			      includeTree   ,
			      false         , // add to cache?
			      0             , // max cache age
			      startFileNum  ,
			      numFiles      ,
			      NULL          , // state
			      NULL          , // callback
			      0             , // niceness
			      false         , // err correction?
			      NULL          , // cache key ptr
			      0             , // retry num
			      -1            , // maxRetries
			      true          , // compensate for merge
			      -1LL          , // sync point
			      &msg5b        )){
		log(LOG_LOGIC,"db: getList did not block.");
		return;
	}
	// all done if empty
	if ( list.isEmpty() ) return;
	// loop over entries in list
	for ( list.resetListPtr() ; ! list.isExhausted() ;
	      list.skipCurrentRecord() ) {
		key_t k       = list.getCurrentKey();
		char *rec     = list.getCurrentRec();
		int32_t  recSize = list.getCurrentRecSize();
		//int64_t docId       = g_titledb.getDocIdFromKey ( k );
		if ( k <= lastKey ) 
			log("key out of order. "
			    "lastKey.n1=%"XINT32" n0=%"XINT64" "
			    "currKey.n1=%"XINT32" n0=%"XINT64" ",
			    lastKey.n1,lastKey.n0,
			    k.n1,k.n0);
		lastKey = k;
		// print deletes
// 		if ( (k.n0 & 0x01) == 0) {
// 			fprintf(stderr,"n1=%08"XINT32" n0=%016"XINT64" docId=%012"INT64" "
// 			       "hh=%07"XINT32" ch=%08"XINT32" (del)\n", 
// 			       k.n1 , k.n0 , docId , hostHash , contentHash );
// 			continue;
// 		}
		// uncompress the title rec
		TitleRec tr;
		if ( ! tr.set ( rec , recSize , false ) )
			continue;
		Xml xml;
		char *s    = tr.getContent();
		int32_t  slen = tr.getContentLen();
		int16_t csEnum = tr.getCharset();
		if ( ! xml.set ( csEnum, s , slen , 
				 false , // ownData?
				 0, 
				 false, 
				 tr.getVersion() ) ) 
			continue;

		Links links;

		Url *linker = tr.getRedirUrl();
		//Xml *sx = g_tagdb.getSiteXml ( tr.getSiteFilenum(),
		//				coll , //tr.getColl() , 
		//				collLen);//tr.getCollLen());
		links.set ( true , &xml , linker , false, // includeLinkHashes
	true, TITLEREC_CURRENT_VERSION, // true=useBaseHref?
			    0 );


		Words words;
		words.set(&xml, true, 0);

		log(LOG_WARN, "looking at %s", tr.getUrl()->getUrl());

	}
	startKey = *(key_t *)list.getLastKey();
	startKey += (uint32_t) 1;
	// watch out for wrap around

	if ( startKey < *(key_t *)list.getLastKey() ) {
		return;
	}

	goto loop;
}


// Run automated qa test showing the differences between servers located at
// s1 and s2.
// u: optional filename of list of urls to check for parse diffs
// q: optional filename of list of queries to check for result diffs

void qaTest ( char *s1, char *s2, char *u, char *q) {
	QAClient qaClient;
	qaClient.init(s1, s2, u, q);
	
	//qaClient.parseUrls(urlList);
	//qaClient.diffQueries(queryList);
	// Crap, we need a loop
	qaClient.runTests();
	
}
// Need a test for the diff method used in qa test
void xmlDiffTest(char *file1, char *file2, DiffOpt *opt){
	diffXmlFiles(file1, file2, opt);
}
*/


// generate the copies that need to be done to scale from oldhosts.conf
// to newhosts.conf topology.
int collinject ( char *newHostsConf ) {

	g_hostdb.resetPortTables();

	Hostdb hdb;
	//if ( ! hdb.init(newHostsConf, 0/*assume we're zero*/) ) {
	if ( ! hdb.init( 0/*assume we're zero*/) ) {
		log("collinject failed. Could not init hostdb with %s",
		    newHostsConf);
		return -1;
	}

	// ptrs to the two hostdb's
	Hostdb *hdb1 = &g_hostdb;
	Hostdb *hdb2 = &hdb;

	if ( hdb1->m_numHosts != hdb2->m_numHosts ) {
		log("collinject: num hosts differ!");
		return -1;
	}

	// . ensure old hosts in g_hostdb are in a derivate groupId in
	//   newHostsConf
	// . old hosts may not even be present! consider them the same host,
	//   though, if have same ip and working dir, because that would
	//   interfere with a file copy.
	for ( int32_t i = 0 ; i < hdb1->m_numShards ; i++ ) {
		//Host *h1 = &hdb1->getHost(i);//m_hosts[i];
		//int32_t gid = hdb1->getGroupId ( i ); // groupNum
		uint32_t shardNum = (uint32_t)i;

		Host *h1 = hdb1->getShard ( shardNum );
		Host *h2 = hdb2->getShard ( shardNum );
		
		printf("ssh %s 'nohup /w/gbi -w /w/ inject titledb "
		       "%s:%"INT32" >& /w/ilog' &\n"
		       , h1->m_hostname
		       , iptoa(h2->m_ip)
		       //, h2->m_hostname
		       , (int32_t)h2->m_httpPort
		       );
	}
	return 1;
}

bool isRecoveryFutile ( ) {

	// scan logs in last 60 seconds
	Dir dir;
	dir.set ( g_hostdb.m_dir );
	dir.open ();

	// scan files in dir
	char *filename;

	int32_t now = getTimeLocal();

	int32_t fails = 0;

	// getNextFilename() writes into this
	char pattern[8]; strcpy ( pattern , "*"); // log*-*" );

	while ( ( filename = dir.getNextFilename ( pattern ) ) ) {
		// filename must be a certain length
		//int32_t filenameLen = gbstrlen(filename);

		char *p = filename;

		if ( !strstr ( filename,"log") ) continue;

		// skip "log"
		p += 3;
		// skip digits for hostid
		while ( isdigit(*p) ) p++;

		// skip hyphen
		if ( *p != '-' ) continue;
		p++;

		// open file
		File ff;
		ff.set ( dir.getDir() , filename );
		// skip if 0 bytes or had error calling ff.getFileSize()
		int32_t fsize = ff.getFileSize();
		if ( fsize == 0 ) continue;
		ff.open ( O_RDONLY );
		// get time stamp
		int32_t timestamp = ff.getLastModifiedTime ( );

		// skip if not iwthin 2 minutes
		if ( timestamp < now - 2*60 ) continue;

		// open it up to see if ends with sighandle
		int32_t toRead = 3000;
		if ( toRead > fsize ) toRead = fsize;
		char mbuf[3002];
		ff.read ( mbuf , toRead , fsize - toRead );

		bool failedToStart = false;

		if ( strstr (mbuf,"sigbadhandler") ) failedToStart = true;
		if ( strstr (mbuf,"Failed to bind") ) failedToStart = true;

		if ( ! failedToStart ) continue;

		// count it otherwise
		fails++;
	}

	// if we had less than 5 failures to start in last 60 secs
	// do not consider futile
	if ( fails < 5 ) return false;

	log("process: KEEP ALIVE LOOP GIVING UP. Five or more cores in "
	    "last 60 seconds.");

	// otherwise, give up!
	return true;
}

char *getcwd2 ( char *arg2 ) {

	// get full absolute non-symlink path from /proc/<pid>
	/*
	pid_t pid = getpid();
	char ff[128];
	sprintf(ff,"/proc/%"INT64"/cmdline",(int64_t)pid);

	int fd = open ( ff , O_RDONLY, 0 );
	if ( ! fd ) return NULL;

	static char s_cmdline[1024];
	int32_t len = read ( fd , s_cmdline, 990 );
	if ( len<=0 || len > 1000 ) return NULL;

	// take the /gb off the end
	char *cend = s_cmdline + gbstrlen(s_cmdline)-1;
	while ( cend>s_cmdline && *cend!='/' ) cend--;
	if ( cend > s_cmdline ) end[1] = '\0';

	return s_cmdline;
	*/


	// test it
	//arg2 = "/bin/gb";

	//fprintf(stderr,"arg2=%s\n",arg2);


	char argBuf[1026];
	char *arg = argBuf;

	//
	// arg2 examples:
	// ./gb
	// /bin/gb (symlink to ../../var/gigablast/data0/gb)
	// /usr/bin/gb (symlink to ../../var/gigablast/data0/gb)
	//

	// 
	// if it is a symbolic link...
	// get real path (no symlinks symbolic links)
	char tmp[1026];
	int32_t tlen = readlink ( arg2 , tmp , 1020 );
	// if we got the actual path, copy that over
	if ( tlen != -1 ) {
		//fprintf(stderr,"tmp=%s\n",tmp);
		// if symbolic link is relative...
		if ( tmp[0]=='.' && tmp[1]=='.') {
			// store original path (/bin/gb --> ../../var/gigablast/data/gb)
			strcpy(arg,arg2); // /bin/gb
			// back up to /
			while(arg[gbstrlen(arg)-1] != '/' ) arg[gbstrlen(arg)-1] = '\0';
			int32_t len2 = gbstrlen(arg);
			strcpy(arg+len2,tmp);
		}
		else {
			strcpy(arg,tmp);
		}
	}
	else {
		strcpy(arg,arg2);
	}

 again:
	// now remove ..'s from path
	char *p = arg;
	// char *start = arg;
	for ( ; *p ; p++ ) {
		if (p[0] != '.' || p[1] !='.' ) continue;
		// if .. is at start of string
		if ( p == arg ) {
			gbmemcpy ( arg , p+2,gbstrlen(p+2)+1);
			goto again;
		}
		// find previous /
		char *slash = p-1;
		if ( *slash !='/' ) { char *xx=NULL;*xx=0; }
		slash--;
		for ( ; slash > arg && *slash != '/' ; slash-- );
		if ( slash<arg) slash=arg;
		gbmemcpy(slash,p+2,gbstrlen(p+2)+1);
		goto again;
		// if can't back up anymore...
	}




	//fprintf(stderr,"arg=%s\n",arg);


	// skip initial . and /
	//if ( arg[0] == '.' && arg[1] == '/' ) arg += 1;

	char *a = arg;

	// remove "gb" from the end
	int32_t alen = 0;
	for ( ; *a ; a++ ) {
		if ( *a != '/' ) continue;
		alen = a - arg + 1;
	}
	if ( alen > 512 ) {
		log("db: path is too long");
		g_errno = EBADENGINEER;
		return NULL;
	}
	// hack off the "gb" (seems to hack off the "/gb")
	//*a = '\0';
	// don't hack off the "/gb" just the "gb"
	arg[alen] = '\0';

	// get cwd which is only relevant to us if arg starts 
	// with . at this point
	static char s_cwdBuf[1025];
	getcwd ( s_cwdBuf , 1020 );
	char *end = s_cwdBuf + gbstrlen(s_cwdBuf);
	// make sure that shit ends in /
	if ( s_cwdBuf[gbstrlen(s_cwdBuf)-1] != '/' ) {
		int32_t len = gbstrlen(s_cwdBuf);
		s_cwdBuf[len] = '/';
		s_cwdBuf[len+1] = '\0';
		end++;
	}

	//fprintf(stderr,"cwdBuf=%s\n",s_cwdBuf);
	//fprintf(stderr,"arg=%s\n",arg);
		

	// if "arg" is a RELATIVE path then append it
	if ( arg && arg[0]!='/' ) {
		if ( arg[0]=='.' && arg[1]=='/' ) {
			gbmemcpy ( end , arg+2 , alen -2 );
			end += alen - 2;
		}
		else {
			gbmemcpy ( end , arg , alen );
			end += alen;
		}
		*end = '\0';
	}
	// if our path started with / then it was absolute...
	else {
		strncpy(s_cwdBuf,arg,alen);
		s_cwdBuf[alen]='\0';
	}

	// make sure it ends in / for consistency
	int32_t clen = gbstrlen(s_cwdBuf);
	if ( s_cwdBuf[clen-1] != '/' ) {
		s_cwdBuf[clen-1] = '/';
		s_cwdBuf[clen] = '\0';
		clen--;
	}

	//fprintf(stderr,"cwdBuf is %s\n",s_cwdBuf);
	// size of the whole thing
	//int32_t clen = gbstrlen(s_cwdBuf);
	// store terminating /
	//if ( clen < 1024 ) {
	//	s_cwdBuf[clen] = '/';
	//	s_cwdBuf[clen+1] = '\0';
	//}

	//log("hey: hey %s",s_cwdBuf);

	// ensure 'gb' binary exists in that dir. 
	// binaryCmd is usually gb but use this just in case
	char *binaryCmd = arg2 + gbstrlen(arg2) - 1;
	for ( ; binaryCmd[-1] && binaryCmd[-1] != '/' ; binaryCmd-- );
	File fff;
	fff.set (s_cwdBuf,binaryCmd);

	// assume it is in the usual spot
	if ( fff.doesExist() ) return s_cwdBuf;

	// try just "gb" as binary
	fff.set(s_cwdBuf,"gb");
	if ( fff.doesExist() ) return s_cwdBuf;

	// if nothing is found resort to the default location
	return "/var/gigablast/data0/";
}

///////
//
// used to make package to install files for the package
//
///////
int copyFiles ( char *dstDir ) {

	char *srcDir = "./";
	SafeBuf fileListBuf;
	g_process.getFilesToCopy ( srcDir , &fileListBuf );

	// include data files so when building a debian/redhat
	// package 'make install' we copy those as well.
	// no let's just build it the first time gb runs, but gb should
	// bind to port 8000 before building and just return a msg
	// that says "pls wait while building data files for the first time"
	//File f;
	//f.set ( srcDir ,"wikititles2.dat");
	//if ( f.doesExist() ) 
	//	fileListBuf.safePrintf(" %s",f.getFilename());

	SafeBuf tmp;
	tmp.safePrintf(
		       "cp -r %s %s"
		       , fileListBuf.getBufStart()
		       , dstDir 
		       );

	//log(LOG_INIT,"admin: %s", tmp.getBufStart());
	fprintf(stderr,"\nRunning cmd: %s\n",tmp.getBufStart());
	system ( tmp.getBufStart() );
	return 0;
}

void rmTest() {

	// make five files
	int32_t max = 100;

	for ( int32_t i = 0 ; i < max ; i++ ) {
		SafeBuf fn;
		fn.safePrintf("./tmpfile%"INT32"",i);
		SafeBuf sb;
		for ( int32_t j = 0 ; j < 100 ; j++ ) {
			sb.safePrintf("%"INT32"\n",(int32_t)rand());
		}
		sb.save ( fn.getBufStart() );
	}

	// now delete
	fprintf(stderr,"Deleting files\n");
	int64_t now = gettimeofdayInMilliseconds_force();

	for ( int32_t i = 0 ; i < max ; i++ ) {
		SafeBuf fn;
		fn.safePrintf("./tmpfile%"INT32"",i);
		File f;
		f.set ( fn.getBufStart() );
		f.unlink();
	}

	int64_t took = gettimeofdayInMilliseconds_force() - now;

	fprintf(stderr,"Deleting files took %"INT64" ms\n",took);

}
