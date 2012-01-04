/*
 * Copyright (c) 1996-2011 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * This code has nothing to do with BG/Q transactional memory!
 *
 * This code groups multiple debugger operations into single packets
 * (each BG/Q packet can support up to 16 debugger commands).  We 
 * implement this by starting a transaction, issuing a set of debugger
 * commands and then sending the packet when everything is ready.
 **/
#include "external/bluegene/MessageHeader.h"
#include "external/bluegene/ToolctlMessages.h"

template <class CmdType, class AckType>
class Transaction
{
  private:
   bgq_process *proc;
   bool temporary_transaction;
   signed int running_transaction;
   unsigned transaction_index;
   char *packet_buffer;
   size_t packet_buffer_size;
   size_t packet_buffer_maxsize;
   uint32_t rank;
   uint16_t msg_id;
   set_response::ptr set_resp;

  private:
   bool flushTransaction()
   {
      assert(running_transaction);
      if (!transaction_index)
         return true;

      assert(set_resp);
      getResponses().addResponse(set_resp, proc);
      set_resp = set_response::ptr();

      CmdType *msg = (CmdType *) packet_buffer;
      msg->header.length = packet_buffer_size;

      proc->getComputeNode()->writeToolMessage(proc, msg, true);

      resetTransactionState();
      return true;
   }
   
   void growTransactionBuffer(size_t size)
   {
      if (size <= packet_buffer_maxsize)
         return;
      size += 64; //little extra padding to reduce growth operations
      packet_buffer = (char *) realloc(packet_buffer, size);
      assert(packet_buffer);
      packet_buffer_maxsize = size;
   }

   size_t copyToTransactionBuffer(char *src, size_t size)
   {
      growTransactionBuffer(size + packet_buffer_size);
      memcpy(packet_buffer + packet_buffer_size, src, size);
      size_t orig_size = packet_buffer_size;
      packet_buffer_size += size;
      return orig_size;
   }

  public:

   Transaction(bgq_process *p, uint16_t mid) :
     proc(p),
     temporary_transaction(false),
     running_transaction(0),
     transaction_index(0),
     packet_buffer(NULL),
     packet_buffer_size(0),
     packet_buffer_maxsize(0),
     rank(p->getRank()),
     msg_id(mid)
   {
   }
        

   bool beginTransaction()
   {
      if (!temporary_transaction)
         pthrd_printf("Beginning transaction (%d) for rank %d\n", running_transaction, proc->getRank());
      
      running_transaction++;
      return true;
   }

   bool endTransaction()
   {
      if (!temporary_transaction)
         pthrd_printf("Ending transaction (%d) for rank %d\n", running_transaction, proc->getRank());
      
      running_transaction--;
      assert(running_transaction >= 0);
      bool result = true;
      if (!running_transaction && transaction_index) {
         result = flushTransaction();
      }
      return result;
   }

   CmdType &getCommand()
   {
      return *((CmdType *) packet_buffer);
   }

   bool writeCommand(const ToolCommand *cmd, uint16_t cmd_type, response::ptr resp)
   {
      uint16_t msg_type = bgq_process::getCommandMsgType(cmd_type);
      assert(msg_type == msg_id);

      if (!running_transaction) {
         //If we're not in a transaction, start a temporary one for this one command.
         temporary_transaction = true;
         beginTransaction();
      }
      size_t cmd_size = bgq_process::getCommandLength(cmd_type, *cmd);
      unsigned int next_start_offset;
      CmdType *msg;
      if (!transaction_index) {
         assert(!set_resp);
         set_resp = set_response::createSetResponse();
         
         assert(packet_buffer_size == 0);
         growTransactionBuffer(sizeof(CmdType) + cmd_size);
         msg = (CmdType *) packet_buffer;;
         msg->numCommands = 0;
         msg->header.service = ToolctlService;
         msg->header.version = ProtocolVersion;
         msg->header.type = msg_id;
         msg->header.rank = rank;
         msg->header.sequenceId = resp->getID();
         msg->header.returnCode = 0;
         msg->header.errorCode = 0;
         msg->header.length = 0; //Fill in after packet is ready
         msg->header.jobId = bgq_process::getJobID();
         msg->toolId = bgq_process::getToolID();
         next_start_offset = sizeof(CmdType);
      }
      else {
         msg = (CmdType *) packet_buffer;
         CommandDescriptor &last_cmd = msg->cmdList[transaction_index-1];
         next_start_offset = last_cmd.offset + last_cmd.length;
      }

      growTransactionBuffer(next_start_offset + cmd_size);
      memcpy(packet_buffer + next_start_offset, &cmd, cmd_size);
      CommandDescriptor &this_cmd = msg->cmdList[transaction_index];
      this_cmd.type = cmd_type;
      this_cmd.reserved = 0;
      this_cmd.offset = next_start_offset;
      this_cmd.length = cmd_size;
      this_cmd.returnCode = 0;
      set_resp->addResp(resp);
      transaction_index++;

      if (temporary_transaction || bgq_process::isActionCommand(cmd_type)) {
         //If we started a temporary transaction, then end it.
         return endTransaction();
      }
      return true;
   }

   void resetTransactionState()
   {
      packet_buffer_size = 0;
      transaction_index = 0;
   }
};
