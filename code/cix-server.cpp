// $Id: cix-server.cpp,v 1.3 2014-05-30 12:47:58-07 - - $

#include <iostream>
#include <fstream>
using namespace std;

#include <libgen.h>

#include "cix_protocol.h"
#include "logstream.h"
#include "signal_action.h"
#include "sockets.h"

logstream log (cout);

void reply_ls (accepted_socket& client_sock, cix_header& header) {
   FILE* ls_pipe = popen ("ls -l", "r");
   if (ls_pipe == NULL) {
      log << "ls -l: popen failed: " << strerror (errno) << endl;
      header.cix_command = CIX_NAK;
      header.cix_nbytes = errno;
      send_packet (client_sock, &header, sizeof header);
   }
   string ls_output;
   char buffer[0x1000];
   for (;;) {
      char* rc = fgets (buffer, sizeof buffer, ls_pipe);
      if (rc == nullptr) break;
      ls_output.append (buffer);
   }
   header.cix_command = CIX_LSOUT;
   header.cix_nbytes = ls_output.size();
   memset (header.cix_filename, 0, CIX_FILENAME_SIZE);
   log << "sending header " << header << endl;
   send_packet (client_sock, &header, sizeof header);
   send_packet (client_sock, ls_output.c_str(), ls_output.size());
   log << "sent " << ls_output.size() << " bytes" << endl;
}

void reply_get(accepted_socket& client_sock, cix_header& header){
	string filen(header.cix_filename);	
	ifstream ifile(filen);
	
	if(ifile){
		header.cix_command = CIX_FILE;
		
		ifile.seekg (0, ifile.end);
		uint32_t l = ifile.tellg();
		ifile.seekg (0, ifile.beg);		
		char buffer[l];
		
		ifile.read(buffer, l);
		log << "sent " << l+1 << " bytes" << endl;
		header.cix_nbytes = l;
		send_packet (client_sock, &header, sizeof header);
		send_packet (client_sock, buffer, header.cix_nbytes);
	}
	else{
		log << "get failed, file doesnt exist " << strerror (errno) << endl;
      header.cix_command = CIX_NAK;
      header.cix_nbytes = errno;
      send_packet (client_sock, &header, sizeof header);
	}
}

void handle_put(accepted_socket& client_sock, cix_header& header){
	string filen(header.cix_filename);
	ofstream ofile(filen);
	char buffer[header.cix_nbytes+1];
	
	recv_packet (client_sock, buffer, header.cix_nbytes);
	
	log << "received " << header.cix_nbytes << " bytes" << endl;
	buffer[header.cix_nbytes] = '\0';

	if(ofile){
		ofile.write(buffer, header.cix_nbytes);
		header.cix_command = CIX_ACK;
		memset (header.cix_filename, 0, CIX_FILENAME_SIZE);
		log << "sending header " << header << endl;
		send_packet (client_sock, &header, sizeof header);
	}
	else{
		log << "Unable to create file " << header.cix_filename << " in cix_get!" << endl;
	}
}

void handle_rm(accepted_socket& client_sock, cix_header& header){
	int e = unlink(header.cix_filename);
	
	if(!e){
		header.cix_command = CIX_ACK;
		log << "sending header " << header << endl;
		send_packet (client_sock, &header, sizeof header);
	}
	else{
		header.cix_command = CIX_NAK;
		log << "sending header " << header << endl;
		send_packet (client_sock, &header, sizeof header);
	}
}


void signal_handler (int signal) {
   log << "signal_handler: caught " << strsignal (signal) << endl;
   switch (signal) {
      case SIGINT: case SIGTERM: throw cix_exit();
      default: break;
   }
}

int main (int argc, char**argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   signal_action (SIGINT, signal_handler);
   signal_action (SIGTERM, signal_handler);
   int client_fd = stoi (args[0]);
   log << "starting client_fd " << client_fd << endl;
   try {
      accepted_socket client_sock (client_fd);
      log << "connected to " << to_string (client_sock) << endl;
      for (;;) {
         cix_header header;
         recv_packet (client_sock, &header, sizeof header);
         log << "received header " << header << endl;
         switch (header.cix_command) {
            case CIX_LS:
               reply_ls (client_sock, header);
               break;
			case CIX_GET:
				reply_get (client_sock, header);
				break;
			case CIX_PUT:
				handle_put (client_sock, header);
				break;
			case CIX_RM:
				handle_rm (client_sock, header);
				break;
            default:
               log << "invalid header from client" << endl;
               log << "cix_nbytes = " << header.cix_nbytes << endl;
               log << "cix_command = " << header.cix_command << endl;
               log << "cix_filename = " << header.cix_filename << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      log << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   return 0;
}

