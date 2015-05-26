// $Id: cix-client.cpp,v 1.4 2014-05-30 12:47:58-07 - - $

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include "cix_protocol.h"
#include "logstream.h"
#include "signal_action.h"
#include "sockets.h"

logstream log (cout);

unordered_map<string,cix_command> command_map {
   {"exit", CIX_EXIT},
   {"help", CIX_HELP},
   {"ls"  , CIX_LS  },
   {"get" , CIX_GET },
   {"put" , CIX_PUT },
   {"rm"  , CIX_RM  },
};

void cix_help() {
   static vector<string> help = {
      "exit         - Exit the program.  Equivalent to EOF.",
      "get filename - Copy remote file to local host.",
      "help         - Print help summary.",
      "ls           - List names of files on remote server.",
      "put filename - Copy local file to remote host.",
      "rm filename  - Remove file from remote server.",
   };
   for (const auto& line: help) cout << line << endl;
}

void set_filename (string str, cix_header& header){
	for(uint i=0; i<str.length(); ++i){
		header.cix_filename[i] = str.at(i);
	}
}

void cix_ls (client_socket& server) {
   cix_header header;
   header.cix_command = CIX_LS;
   log << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   log << "received header " << header << endl;
   if (header.cix_command != CIX_LSOUT) {
      log << "sent CIX_LS, server did not return CIX_LSOUT" << endl;
      log << "server returned " << header << endl;
   }else {
      char buffer[header.cix_nbytes + 1];
      recv_packet (server, buffer, header.cix_nbytes);
      log << "received " << header.cix_nbytes << " bytes" << endl;
      buffer[header.cix_nbytes] = '\0';
      cout << buffer;
   }
}

void cix_get (client_socket& server, string arg){
	cix_header header;
	header.cix_command = CIX_GET;
	header.cix_nbytes = 0;
	set_filename(arg, header);
	
	log << "sending header " << header << endl;
	send_packet (server, &header, sizeof header);
	recv_packet (server, &header, sizeof header);
	log << "received header " << header << endl;
	if (header.cix_command != CIX_FILE) {
	  log << "sent CIX_LS, server did not return CIX_LSOUT" << endl;
	  log << "server returned " << header << endl;
	}
	else {
	  char buffer[header.cix_nbytes + 1];
	  recv_packet (server, buffer, header.cix_nbytes);
	  log << "received " << header.cix_nbytes << " bytes" << endl;
	  buffer[header.cix_nbytes] = '\0';
	  
	  string filen(header.cix_filename);
	  
	  ofstream ofile(filen);
	  if(ofile){
		ofile.write(buffer, header.cix_nbytes);
	  }
	  else{
		log << "Unable to create file " << header.cix_filename << " in cix_get!" << endl;
	  }
	}
}

void cix_put (client_socket& server, string arg){
	cix_header header;
	header.cix_command = CIX_PUT;
	
	ifstream ifile(arg);
	
	if(ifile){
		set_filename(arg, header);
		
		ifile.seekg (0, ifile.end);
		uint32_t l = ifile.tellg();
		ifile.seekg (0, ifile.beg);		
		char buffer[l];
		ifile.read(buffer, l);
		
		header.cix_nbytes = l;
		
		log << "sending header " << header << endl;
		send_packet (server, &header, sizeof header);
		send_packet (server, buffer, l);
		
		recv_packet (server, &header, sizeof header);
		log << "received header " << header << endl;
		if (header.cix_command != CIX_ACK) {
		  log << "sent CIX_PUT, server did not return CIX_ACK" << endl;
		  log << "server returned " << header << endl;
		}
		else {
			log << "server successfully returned CIX_ACK" << endl;
		}
	}
	else{
		log << arg << " doesnt exist!" << endl;
		throw cix_exit();
	}
}

void cix_rm (client_socket& server, string arg){
	cix_header header;
	header.cix_command = CIX_RM;
	
	ifstream ifile(arg);
	
	if(ifile){
		set_filename(arg, header);		
		header.cix_nbytes = 0;
		
		log << "sending header " << header << endl;
		send_packet (server, &header, sizeof header);		
		recv_packet (server, &header, sizeof header);
		log << "received header " << header << endl;
		
		if (header.cix_command != CIX_ACK) {
		  log << "sent CIX_PUT, server did not return CIX_ACK" << endl;
		  log << "server returned " << header << endl;
		}
		else {
			log << "server successfully returned CIX_ACK" << endl;
		}
	}
	else{
		log << arg << " doesnt exist!" << endl;
		throw cix_exit();
	}
}


void usage() {
   cerr << "Usage: " << log.execname() << " [host] [port]" << endl;
   throw cix_exit();
}

void signal_handler (int signal) {
   log << "signal_handler: caught " << strsignal (signal) << endl;
   switch (signal) {
      case SIGINT: case SIGTERM: throw cix_exit();
      default: break;
   }
}

vector<string> split (const string& line, const string& delimiters) {
   vector<string> words;
   int end = 0;
   for (;;) {
      size_t start = line.find_first_not_of (delimiters, end);
      if (start == string::npos) break;
      end = line.find_first_of (delimiters, start);
      words.push_back (line.substr (start, end - start));
   }
   return words;
}

int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   vector<string> sline;
   signal_action (SIGINT, signal_handler);
   signal_action (SIGTERM, signal_handler);
   if (args.size() > 2) usage();
   string host = get_cix_server_host (args, 0);
   in_port_t port = get_cix_server_port (args, 1);
   log << to_string (hostinfo()) << endl;
   try {
      log << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      log << "connected to " << to_string (server) << endl;
      for (;;) {
         string line;
         getline (cin, line);
		 sline = split(line, " ");
         if (cin.eof()) throw cix_exit();
         log << "command " << line << endl;
		 //Add spliting of line into command and possible filename
		 
         const auto& itor = command_map.find (sline.at(0));
         cix_command cmd = itor == command_map.end()
                         ? CIX_ERROR : itor->second;
         switch (cmd) {
            case CIX_EXIT:
               throw cix_exit();
               break;
            case CIX_HELP:
               cix_help();
               break;
            case CIX_LS:
               cix_ls (server);
               break;
			case CIX_GET:
				if(sline.size() == 2)
					cix_get (server, sline.at(1));
				else
					log << "incorrect number of arguements for get" << endl;
				break;
			case CIX_PUT:
				if(sline.size() == 2)
					cix_put (server, sline.at(1));
				else
					log << "incorrect number of arguements for put" << endl;
				break;
			case CIX_RM:
				if(sline.size() == 2)
					cix_rm (server, sline.at(1));
				else
					log << "incorrect number of arguements for rm" << endl;
				break;
            default:
               log << line << ": invalid command" << endl;
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

