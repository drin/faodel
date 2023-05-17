// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <string.h> //for memcpy

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"

#include "../../opbox-example-support/Globals.hh"
#include "OpRdmaPing.hh"

//The Globals class just holds basic communication vars we use in these
//examples (ie mpi ranks, etc). It has a generic hook for starting/stopping
//all nodes in this mpi run to make the OpBox codes easier to understand.
Globals G;


//The Faodel uses a plain-text configuration string to set
//different parameters in the stack. The below string defines what
//port the whookie sever will listen on, the type of directory management
//service to employ, and whether internal components should spew their
//debug information or not. If things don't work, try turning on the debug
//info to get a better idea of where things are breaking

std::string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server
net.transport.name   mpi

master.whookie.port   7777
server.whookie.port   1992

dirman.type           centralized
dirman.root_role      master

#target.dirman.host_root
#target.dirman.write_to_file ./dirman.txt

#bootstrap.debug true
#whookie.debug   true
#opbox.debug     true
#dirman.debug    true

#nnti.logger.severity debug

)EOF";

using namespace std;


int main(int argc, char **argv){

  //This is a simple example of how to launch a small RDMA ping-pong communication
  cout <<"My simple RDMA ping example\n";

  //First, we need to register our new Op so OpBox will know how to handle a
  //particular Op whenever it sees a user request or message relating to it.
  //The RegisterOp template examines the class you give it by creating and
  //examining a dummy instance of the class to see what its id/name are.
  //
  //Ideally, we want to do registration *before* we start up opbox because
  //pre-init registrations can be maintained in a special list that doesn't
  //incur locking overheads at runtime. You *can* register ops after the
  //OpBox is started. However, those ops (1) incur a mutex and (2) won't be
  //known to OpBox until they're registered.
  opbox::RegisterOp<OpRdmaPing>();

  //Next, we want to create a master config that tells all the faodel
  //components how they should be configured. For the examples, the Global
  //class modifies the config so it can designate rank 0 as the master and
  //any other nodes as servers.
  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();

  G.StartAll(argc, argv, config);

  if(G.mpi_size==1) {
    std::cerr<<"This example needs to be run with multiple mpi ranks\n";
    G.StopAll();
    return -1;
  }

  //For this example, the master node is going to create a new Op and
  //launch it. The flow of operation is as follows:
  //  Origin (user)  : Allocate a Lunasa RDMA buffer and fill with a message
  //  Origin (user)  : Create a new Op
  //  Origin (user)  : Get a future from the op in order to get the final result
  //  Origin (user)  : Hand over the op to opbox for execution
  //  Origin (opbox) : Create a message, send it to the destination, wait for reply
  //  Target (opbox) : Observe a new message, create a new Op for processing it
  //  Target (Op)    : Extract message, RDMA get message
  //  Target (Op)    : Transform message, RDMA put message
  //  Target (Op)    : Create ACK message, and send it
  //  Target (opbox) : Destroy op, Destroy ACK message when sending completes
  //  Origin (opbox) : Observe a new message, match it to the op that sent it
  //  Origin (op)    : Extract message, pass it to future, tell opbox we're done
  //  Origin (opbox) : Destroy the op
  //  Origin (user)  : future available, provide result to user
  if (G.mpi_rank==0) {
    std::string ping_str("this is the ping message");
    cout<<"Original message is '"<<ping_str<<"'"<<endl;

    // allocate an Lunasa DataObject (LDO) to be the target of RDMA operations
    lunasa::DataObject ping_ldo = lunasa::DataObject(0, ping_str.size()+1, lunasa::DataObject::AllocatorType::eager);
    // copy the message into the RDMA target
    memcpy(ping_ldo.GetDataPtr(), ping_str.c_str(), ping_str.size()+1);
    // create the op.  all we need to tell it is the peer to ping and
    // the RDMA target LDO.
    OpRdmaPing *op1 = new OpRdmaPing(G.peers[1], ping_ldo);
    // the op creates a future that we can use to wait for the op to complete
    future<string> fut1 = op1->GetFuture();

    opbox::LaunchOp(op1);

    // when the op completes it will set the future to the transformed
    // ping message
    string res1=fut1.get();

    cout<<"Result message is '"<<res1<<"'\n";
  }

  //Finally, do an mpi barrier to sync all nodes and then invoke shutdown procedures
  //to stop the Faodel. Global also does an mpi finalize to close out the test.
  G.StopAll();

  return 0;
}
