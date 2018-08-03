//
// Created by oiluz on 9/4/2017.
//

#include <guacservice/GuacClientUser.h>

GuacClientUser::GuacClientUser(guac_user * pUser, const std::string & stShmName)
        : m_User(pUser), m_stShmName(stShmName), m_AudioMimeTypes(nullptr), m_VideoMimeTypes(nullptr),
          m_ImageMimeTypes(nullptr), m_bIsRunning(false)
{
   m_Parser = guac_parser_alloc();
   m_User->socket = guac_socket_shared_memory_socket_join(m_stShmName, true, true);
}

void GuacClientUser::IOThread()
{
   m_bIsRunning = true;
   // Guacamole user input loop
   GuacLogger::GetInstance()->Debug() << "Running IO Loop [" << m_User->client->connection_id << "]";
   while(m_User->client->state == GUAC_CLIENT_RUNNING && m_User->active && m_bIsRunning)
   {
      // Read instruction, stop on error
      if(guac_parser_read(m_Parser, m_User->socket, 5000))
      {
         if(guac_error == GUAC_STATUS_TIMEOUT)
         {
            GuacLogger::GetInstance()->Error() << "Timeout on parser read [" << m_User->client->connection_id << "]";
            guac_user_abort(m_User, GUAC_PROTOCOL_STATUS_CLIENT_TIMEOUT, "User is not responding.");
         }
         else
         {
            if(guac_error != GUAC_STATUS_CLOSED)
            {
               GuacLogger::GetInstance()->Error() << "Guacamole connection failure"
                                                  << " [" + m_User->client->connection_id << "]";
            }
            else
            {
               GuacLogger::GetInstance()->Error() << "Guacamole Resource Closed" << " ["
                                                  << m_User->client->connection_id
                                                  << "]";
            }
         }

         return;
      }

      // Reset guac_error and guac_error_message (user/client handlers are not guaranteed to set these)
      guac_error = GUAC_STATUS_SUCCESS;
      guac_error_message = nullptr;

      // Call handler, stop on error
      if(guac_user_handle_instruction(m_User, m_Parser->opcode, m_Parser->argc, m_Parser->argv) < 0)
      {
         GuacLogger::GetInstance()->Error() << "Failed handling instruction [" << m_User->client->connection_id << "]";
         guac_user_stop(m_User);
         return;
      }
   }
   GuacLogger::GetInstance()->Debug() << "Finished IO Loop [" << m_User->client->connection_id << "]";
}

void GuacClientUser::FreeMimeTypes(char ** mimetypes)
{
   char ** current_mimetype = mimetypes;

   // Free all strings within NULL-terminated mimetype array 
   while(*current_mimetype)
   {
      delete *current_mimetype;
      current_mimetype++;
   }

   // Free the array itself, now that its contents have been freed 
   delete mimetypes;
}

char ** GuacClientUser::CopyMimeTypes()
{
   // Allocate sufficient space for NULL-terminated array of mimetypes 
   char ** mimetypes_copy = new char * [m_Parser->argc + 1];

   // Copy each provided mimetype 
   for(int i = 0; i < m_Parser->argc; i++)
   {
      mimetypes_copy[i] = _strdup(m_Parser->argv[i]);
   }

   // Terminate with NULL 
   mimetypes_copy[m_Parser->argc] = NULL;

   return mimetypes_copy;
}

bool GuacClientUser::Expect(const std::string & stInstruction, int iArgc)
{
   // Expects the parser to receive the given instruction from the upcoming socket packets
   if(guac_parser_expect(m_Parser, m_User->socket, GUACD_TIMEOUT, stInstruction.c_str()))
   {
      GuacLogger::GetInstance()->Error() << "Expect Failure for " << stInstruction
                                         << " [" + m_User->client->connection_id << "]";
      guac_parser_free(m_Parser);
      return false;
   }

   // Asserts that the instruction has the needed size of args
   if(iArgc != 0 && m_Parser->argc < iArgc)
   {
      GuacLogger::GetInstance()->Error() << "Expect Failure with Argc For " << stInstruction
                                         << " [" + m_User->client->connection_id << "]";
      guac_parser_free(m_Parser);
      return false;
   }

   return true;
}

bool GuacClientUser::SendClientArgs()
{
   // Send the client args to begin the handshake
   if(guac_protocol_send_args(m_User->socket, m_User->client->args) || guac_socket_flush(m_User->socket))
   {
      return false;
   }
   return true;
}

bool GuacClientUser::PerformHandshake()
{
   // Try to read size
   GuacLogger::GetInstance()->Debug() << "Trying to read size instruction [" << m_User->client->connection_id << "]";
   if(!Expect("size", 2))
   {
      GuacLogger::GetInstance()->Error() << "Failed reading size instruction [" << m_User->client->connection_id << "]";
      return false;
   }

   m_User->info.optimal_width = atoi(m_Parser->argv[0]);
   m_User->info.optimal_height = atoi(m_Parser->argv[1]);

   if(m_Parser->argc >= 3)
   {
      m_User->info.optimal_resolution = atoi(m_Parser->argv[2]);
   }
   else
   {
      m_User->info.optimal_resolution = GUAC_DEFAULT_OPTIMAL_RESOLUTION;
   }

   // Try to read audio
   GuacLogger::GetInstance()->Debug() << "Trying to read audio instruction [" << m_User->client->connection_id << "]";
   if(!Expect("audio", 0))
   {
      GuacLogger::GetInstance()->Error() << "Failed reading audio instruction [" << m_User->client->connection_id
                                         << "]";
      return false;
   }

   m_AudioMimeTypes = CopyMimeTypes();
   m_User->info.audio_mimetypes = (const char **) m_AudioMimeTypes;

   // Try to read video
   GuacLogger::GetInstance()->Debug() << "Trying to read video instruction [" << m_User->client->connection_id << "]";
   if(!Expect("video", 0))
   {
      GuacLogger::GetInstance()->Error() << "Failed reading video instruction [" << m_User->client->connection_id
                                         << "]";
      return false;
   }

   m_VideoMimeTypes = CopyMimeTypes();
   m_User->info.video_mimetypes = (const char **) m_VideoMimeTypes;

   // Try to read supported image types
   GuacLogger::GetInstance()->Debug() << "Trying to read image instruction [" << m_User->client->connection_id << "]";
   if(!Expect("image", 0))
   {
      GuacLogger::GetInstance()->Error() << "Failed reading image instruction [" << m_User->client->connection_id
                                         << "]";
      return false;
   }

   m_ImageMimeTypes = CopyMimeTypes();
   m_User->info.image_mimetypes = (const char **) m_ImageMimeTypes;

   // Read connect instruction
   GuacLogger::GetInstance()->Debug() << "Trying to read connect instruction [" << m_User->client->connection_id << "]";
   if(!Expect("connect", 0))
   {
      GuacLogger::GetInstance()->Error() << "Failed reading connect instruction [" << m_User->client->connection_id
                                         << "]";
      return false;
   }

   // Send that you are ready and attempt to join the user
   GuacLogger::GetInstance()->Debug() << "Sending connection protocol ready [" << m_User->client->connection_id << "]";
   guac_protocol_send_ready(m_User->socket, m_User->client->connection_id.c_str());
   guac_socket_flush(m_User->socket);

   GuacLogger::GetInstance()->Debug() << "Adding user to client [" << m_User->client->connection_id << "]";
   if(guac_client_add_user(m_User->client, m_User, m_Parser->argc, m_Parser->argv))
   {
      GuacLogger::GetInstance()->Error() << "Could not add user to client [" << m_User->client->connection_id << "]";
      return false;
   }
   GuacLogger::GetInstance()->Debug() << "Handshake Finished. [" << m_User->client->connection_id << "]";
   return true;
}

void GuacClientUser::RunUser()
{
   // Send args to the parent process
   GuacLogger::GetInstance()->Debug() << "Sending Client Args [" << m_User->client->connection_id << "]";
   if(!SendClientArgs())
   {
      GuacLogger::GetInstance()->Error() << "Failed Sending client args [" << m_User->client->connection_id << "]";
      return;
   }

   // Perform handshake with the user
   GuacLogger::GetInstance()->Debug() << "Performing handshake [" << m_User->client->connection_id << "]";
   if(!PerformHandshake())
   {
      GuacLogger::GetInstance()->Error() << "Failed performing handshake [" << m_User->client->connection_id << "]";
      return;
   }

   // Start IO
   GuacLogger::GetInstance()->Debug() << "Starting IO Thread [" << m_User->client->connection_id << "]";
   IOThread();
   GuacLogger::GetInstance()->Debug() << "IO Thread over [" << m_User->client->connection_id << "]";

   // Stop the user entirely
   StopUser();
}

void GuacClientUser::StopUser()
{
   guac_socket_reset(m_User->socket);

   GuacLogger::GetInstance()->Debug() << "Stopping User [" << m_User->user_id << "]";
   m_bIsRunning = false;

   guac_user_stop(m_User);

   // Send disconnection
   GuacLogger::GetInstance()->Debug()
           << "Sending protocol disconnection and closing everything [" << m_User->client->connection_id << "]";
   guac_protocol_send_disconnect(m_User->socket);

   // Free user
   GuacLogger::GetInstance()->Debug() << "Removing User [" << m_User->client->connection_id << "]";
   guac_client_remove_user(m_User->client, m_User);

   // Free mimetypes
   GuacLogger::GetInstance()->Debug() << "Freeing MimeTypes [" << m_User->client->connection_id << "]";
   FreeMimeTypes(m_AudioMimeTypes);
   FreeMimeTypes(m_VideoMimeTypes);
   FreeMimeTypes(m_ImageMimeTypes);

   // Free Parser
   GuacLogger::GetInstance()->Debug() << "Freeing Parser [" << m_User->client->connection_id << "]";
   guac_parser_free(m_Parser);

   if(m_User->socket)
   {
      guac_socket_free(m_User->socket);
      m_User->socket = nullptr;
   }

   GuacLogger::GetInstance()->Debug() << "Done Releasing everything";
}

std::string GuacClientUser::GetUserTag() const
{
   return m_stShmName;
}

void GuacClientUser::AbortUser()
{
   m_bIsRunning = false;

   guac_socket_reset(m_User->socket);
}