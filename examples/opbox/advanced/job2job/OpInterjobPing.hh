// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPINTERJOBPING_HH
#define OPINTERJOBPING_HH

#include <future>

#include "opbox/OpBox.hh"
#include "faodel-common/Common.hh"
#include "lunasa/DataObject.hh"


class OpInterjobPing : public opbox::Op {

  enum class State : int  {
      start=0,
      snd_wait_for_reply,
      done };

public:
  OpInterjobPing(opbox::net::peer_ptr_t dst, std::string ping_message);
  OpInterjobPing(op_create_as_target_t t);
  ~OpInterjobPing();

  //Means for passing back the result
  std::future<std::string> GetFuture();

  //Unique name and id for this op
  const static unsigned int op_id;
  const static std::string  op_name;
  unsigned int getOpID() const { return op_id; }
  std::string  getOpName() const { return op_name; }

  WaitingType UpdateOrigin(OpArgs *args);
  WaitingType UpdateTarget(OpArgs *args);

  std::string GetStateName() const;

private:
  State state;
  opbox::net::peer_t *peer;

  std::promise<std::string> ping_promise;

  void createOutgoingMessage(faodel::nodeid_t dst,
                             const mailbox_t &src_mailbox, const mailbox_t &dst_mailbox,
                             const std::string &ping_message);
  lunasa::DataObject ldo_msg;

};

#endif // OPINTERJOBPING_HH
