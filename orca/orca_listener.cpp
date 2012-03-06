#include <stdlib.h>
#include <sstream>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h> 
#include <set> 

#include "ORDataProcManager.hh"
#include "ORLogger.hh"
#include "ORSocketReader.hh"
#include "OROrcaRequestProcessor.hh"
#include "ORServer.hh"
#include "ORHandlerThread.hh"

#include "orca.h"
#include "ds.h"

void handler(int signal);

void* orca_listener(void* arg) {
    OrcaURL* ohi = (OrcaURL*) arg;
    char* orcahost = ohi->host;
    int orcaport = ohi->port;

    std::string label = "OR";
    ORVReader* reader = NULL;

    bool keepAliveSocket = false;
    bool runAsDaemon = false;
    unsigned long timeToSleep = 10; //default sleep time for sockets.
    unsigned int reconnectAttempts = 0; // default reconnect tries for sockets.
    unsigned int portToListenOn = 0;
    unsigned int maxConnections = 5; // default connections accepted by server

    ORHandlerThread* handlerThread = new ORHandlerThread();
    handlerThread->StartThread();

    /***************************************************************************/
    /*   Running orcaroot as a daemon server. */
    /***************************************************************************/
    if (runAsDaemon) {
        /* Here we start listening on a socket for connections. */
        /* We are doing this very simply with a simple fork. Eventually want
           to check number of spawned processes, etc.  */
        std::cout << "Running orcaroot as daemon on port: " << portToListenOn << std::endl;
        pid_t childpid = 0;
        std::set<pid_t> childPIDRecord;

        ORServer* server = new ORServer(portToListenOn);
        /* Starting server, binding to a port. */
        if (!server->IsValid()) {
            std::cout << "Error listening on port " << portToListenOn 
                << std::endl << "Error code: " << server->GetErrorCode()
                << std::endl;
            return NULL;
        }

        signal(SIGINT, &handler);
        while (1) {
            /* This while loop is broken by a kill signal which is well handled
             * by the server.  The kill signal will automatically propagate to the
             * children so we really don't have to worry about waiting for them to
             * die.  */
            while (childPIDRecord.size() >= maxConnections) { 
                /* We've reached our maximum number of child processes. */
                /* Wait for a process to end. */
                childpid = wait3(0, WUNTRACED, 0);
                if(childPIDRecord.erase(childpid) != 1) {
                    /* Something really weird happened. */
                    std::cout << "Ended child process " << childpid 
                        << " not recognized!" << std::endl;
                }
            }
            while((childpid = wait3(0,WNOHANG,0)) > 0) {
                /* Cleaning up any children that may have ended.                   * 
                 * This will just go straight through if no children have stopped. */
                if(childPIDRecord.erase(childpid) != 1) {
                    /* Something really weird happened. */
                    std::cout << "Ended child process " << childpid 
                        << " not recognized!" << std::endl;
                }
            } 
            std::cout << childPIDRecord.size()  << " connections running..." << std::endl;
            std::cout << "Waiting for connection..." << std::endl;
            TSocket* sock = server->Accept(); 
            if (sock == (TSocket*) 0 || sock == (TSocket*) -1 ) {
                // There was an error, or the socket got closed .
                if (!server->IsValid()) return 0;
                continue;
            }
            if(!sock->IsValid()) {
                /* Invalid socket, cycle to wait. */
                delete sock;
                continue;
            }
            if ((childpid = fork()) == 0) {
                /* We are in the child process.  Set up reader and fire away. */
                delete server;
                delete handlerThread;
                handlerThread = new ORHandlerThread;
                handlerThread->StartThread();
                reader = new ORSocketReader(sock, true);
                /* Get out of the while loop */
                break;
            } 
            /* Parent process: wait for next connection. Close our descriptor. */
            std::cout << "Connection accepted, child process begun with pid: " 
                << childpid << std::endl;
            childPIDRecord.insert(childpid);
            delete sock; 
        }

        /***************************************************************************/
        /*  End daemon server code.  */
        /***************************************************************************/
    } else {
        /* Normal running, either connecting to a server or reading in a file. */
        std::cout << "connecting to orca: " << orcahost << ":" << orcaport << std::endl;
        reader = new ORSocketReader(orcahost, orcaport);
    }
    if (!reader->OKToRead()) {
        ORLog(kError) << "Reader couldn't read" << std::endl;
        return NULL;
    }

    std::cout << "Setting up data processing manager..." << std::endl;
    ORDataProcManager dataProcManager(reader);

    /* Declare processors here. */
    OROrcaRequestProcessor orcaReq;
    ORBuilderProcessor builderProcessor(label);

    if (runAsDaemon) {
        /* Add them here if you wish to run them in daemon mode ( not likely ).*/
        dataProcManager.SetRunAsDaemon();
        dataProcManager.AddProcessor(&orcaReq);
    } else {
        /* Add the processors here to run them in normal mode. */
        std::cout << "adding builderproc" << std::endl;
        dataProcManager.AddProcessor(&builderProcessor);
    }

    std::cout << "Start processing..." << std::endl;
    dataProcManager.ProcessDataStream();
    std::cout << "Finished processing..." << std::endl;

    delete reader;
    delete handlerThread;

    return NULL;
}

