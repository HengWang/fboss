/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

extern "C" {
#include <opennsl/types.h>
#include <opennsl/l3.h>
}

#include <folly/dynamic.h>
#include <folly/IPAddress.h>
#include <folly/MacAddress.h>
#include "fboss/agent/types.h"
#include "fboss/agent/state/RouteTypes.h"

#include <boost/noncopyable.hpp>
#include <boost/container/flat_set.hpp>

namespace facebook { namespace fboss {

class BcmSwitch;

class BcmEgressBase : public boost::noncopyable {
 public:
  enum : opennsl_if_t {
    INVALID = -1,
  };
  opennsl_if_t getID() const {
    return id_;
  }
  virtual ~BcmEgressBase() {}
  virtual folly::dynamic toFollyDynamic() const = 0;
 protected:
  explicit BcmEgressBase(const BcmSwitch* hw) : hw_(hw) {}
  const BcmSwitch* hw_;
  opennsl_if_t id_{INVALID};
};

class BcmEgress : public BcmEgressBase {
 public:
  explicit BcmEgress(const BcmSwitch* hw) : BcmEgressBase(hw) {}
  ~BcmEgress() override;
  void program(opennsl_if_t intfId, opennsl_vrf_t vrf,
      const folly::IPAddress& ip, folly::MacAddress mac,
               opennsl_port_t port) {
    return program(intfId, vrf, ip, &mac, port, NEXTHOPS);
  }
  void programToCPU(opennsl_if_t intfId, opennsl_vrf_t vrf,
      const folly::IPAddress& ip) {
    return program(intfId, vrf, ip, nullptr, 0, TO_CPU);
  }
  void programToDrop(opennsl_if_t intfId, opennsl_vrf_t vrf,
      const folly::IPAddress& ip) {
    return program(intfId, vrf, ip, nullptr, 0, DROP);
  }
  /*
   * Serialize to folly::dynamic
   */
  folly::dynamic toFollyDynamic() const override;
  /**
   * Create a TO CPU egress object without any specific interface or address
   *
   * This API is used when a generic TO CPU egress object is needed.
   */
  void programToCPU();

  /*
   * By default, BCM SDK create a drop egress object. It is always the
   * first egress object ID created. If we create a new one, the warm
   * reboot cache code will have trouble to find out which one is supposed
   * to use. Therefore, just use the default one.
   * verifyDropEgressId() is to verify this assumption is correct or not.
   */
  static opennsl_if_t getDropEgressId() {
    return 100000;
  }
  /**
   * Verify if egress ID is programmed as drop or not.
   *
   */
  static void verifyDropEgress(int unit);

  // Returns if the egress object is programmed to drop
  static bool programmedToDrop(const opennsl_l3_egress_t& egr) {
    return egr.flags & OPENNSL_L3_DST_DISCARD;
  }

 private:
  bool alreadyExists(const  opennsl_l3_egress_t& newEgress) const;
  void program(opennsl_if_t intfId, opennsl_vrf_t vrf,
      const folly::IPAddress& ip, const folly::MacAddress* mac,
      opennsl_port_t port, RouteForwardAction action);
  folly::MacAddress mac_;
  opennsl_if_t intfId_{INVALID};
};

class BcmEcmpEgress : public BcmEgressBase {
 public:
  using EgressId = opennsl_if_t;
  using Paths = boost::container::flat_set<opennsl_if_t>;

  explicit BcmEcmpEgress(const BcmSwitch* hw,
      Paths paths) : BcmEgressBase(hw), paths_(paths) {
    program();
  }
  ~BcmEcmpEgress() override;
  bool pathUnreachableHwNotLocked(EgressId path);
  bool pathUnreachableHwLocked(EgressId path);
  bool pathReachableNoHWLock(EgressId path);
  bool pathReachableHwLocked(EgressId path);
  const Paths& paths() const {
    return paths_;
  }
  /*
   * Serialize to folly::dynamic
   */
  folly::dynamic toFollyDynamic() const override;
  /*
   * Update ecmp egress entries in HW
   */
  static bool addEgressIdHwNotLocked(int unit,
      EgressId ecmpId, const Paths& egressIdInSw, EgressId toAdd);
  static bool addEgressIdHwLocked(int unit, EgressId ecmpId,
      const Paths& egressIdInSw, EgressId toAdd);
  static bool removeEgressIdHwNotLocked(int unit, EgressId ecmpId,
      const Paths& paths, EgressId path);
  static bool removeEgressIdHwLocked(int unit, EgressId ecmpId,
      const Paths& paths, EgressId path);
 private:
  void program();
  const Paths paths_;
};

}}
