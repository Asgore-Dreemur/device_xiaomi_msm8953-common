/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef AGPS_H
#define AGPS_H

#include <functional>
#include <list>
#include <MsgTask.h>
#include <gps_extended_c.h>
#include <loc_pla.h>
#include <log_util.h>

/* ATL callback function pointers
 * Passed in by Adapter to AgpsManager */
typedef std::function<void(
        int handle, int isSuccess, char* apn, uint32_t apnLen,
        AGpsBearerType bearerType, AGpsExtType agpsType,
        LocApnTypeMask mask)> AgpsAtlOpenStatusCb;

typedef std::function<void(int handle, int isSuccess)> AgpsAtlCloseStatusCb;

/* Post message to adapter's message queue */
typedef std::function<void(LocMsg* msg)>     SendMsgToAdapterMsgQueueFn;

/* AGPS States */
typedef enum {
    AGPS_STATE_INVALID = 0,
    AGPS_STATE_RELEASED,
    AGPS_STATE_PENDING,
    AGPS_STATE_ACQUIRED,
    AGPS_STATE_RELEASING
} AgpsState;

typedef enum {
    AGPS_EVENT_INVALID = 0,
    AGPS_EVENT_SUBSCRIBE,
    AGPS_EVENT_UNSUBSCRIBE,
    AGPS_EVENT_GRANTED,
    AGPS_EVENT_RELEASED,
    AGPS_EVENT_DENIED
} AgpsEvent;

/* Notification Types sent to subscribers */
typedef enum {
    AGPS_NOTIFICATION_TYPE_INVALID = 0,

    /* Meant for all subscribers, either active or inactive */
    AGPS_NOTIFICATION_TYPE_FOR_ALL_SUBSCRIBERS,

    /* Meant for only inactive subscribers */
    AGPS_NOTIFICATION_TYPE_FOR_INACTIVE_SUBSCRIBERS,

    /* Meant for only active subscribers */
    AGPS_NOTIFICATION_TYPE_FOR_ACTIVE_SUBSCRIBERS
} AgpsNotificationType;

/* Classes in this header */
class AgpsSubscriber;
class AgpsManager;
class AgpsStateMachine;

/* SUBSCRIBER
 * Each Subscriber instance corresponds to one AGPS request,
 * received by the AGPS state machine */
class AgpsSubscriber {

public:
    int mConnHandle;

    /* Does this subscriber wait for data call close complete,
     * before being notified ATL close ?
     * While waiting for data call close, subscriber will be in
     * inactive state. */
    bool mWaitForCloseComplete;
    bool mIsInactive;
    LocApnTypeMask mApnTypeMask;

    inline AgpsSubscriber(
            int connHandle, bool waitForCloseComplete, bool isInactive,
            LocApnTypeMask apnTypeMask) :
            mConnHandle(connHandle),
            mWaitForCloseComplete(waitForCloseComplete),
            mIsInactive(isInactive),
            mApnTypeMask(apnTypeMask) {}
    inline virtual ~AgpsSubscriber() {}

    inline virtual bool equals(const AgpsSubscriber *s) const
    { return (mConnHandle == s->mConnHandle); }

    inline virtual AgpsSubscriber* clone()
    { return new AgpsSubscriber(
            mConnHandle, mWaitForCloseComplete, mIsInactive, mApnTypeMask); }
};

/* AGPS STATE MACHINE */
class AgpsStateMachine {
protected:
    /* AGPS Manager instance, from where this state machine is created */
    AgpsManager* mAgpsManager;

    /* List of all subscribers for this State Machine.
     * Once a subscriber is notified for ATL open/close status,
     * it is deleted */
    std::list<AgpsSubscriber*> mSubscriberList;

    /* Current subscriber, whose request this State Machine is
     * currently processing */
    AgpsSubscriber* mCurrentSubscriber;

    /* Current state for this state machine */
    AgpsState mState;

    AgnssStatusIpV4Cb     mFrameworkStatusV4Cb;
private:
    /* AGPS Type for this state machine
       LOC_AGPS_TYPE_ANY           0
       LOC_AGPS_TYPE_SUPL          1
       LOC_AGPS_TYPE_WWAN_ANY      3
       LOC_AGPS_TYPE_SUPL_ES       5 */
    AGpsExtType mAgpsType;
    LocApnTypeMask mApnTypeMask;

    /* APN and IP Type info for AGPS Call */
    char* mAPN;
    unsigned int mAPNLen;
    AGpsBearerType mBearer;

public:
    /* CONSTRUCTOR */
    AgpsStateMachine(AgpsManager* agpsManager, AGpsExtType agpsType):
        mAgpsManager(agpsManager), mSubscriberList(),
        mCurrentSubscriber(NULL), mState(AGPS_STATE_RELEASED),
        mFrameworkStatusV4Cb(NULL),
        mAgpsType(agpsType), mAPN(NULL), mAPNLen(0),
        mBearer(AGPS_APN_BEARER_INVALID) {};

    virtual ~AgpsStateMachine() { if(NULL != mAPN) delete[] mAPN; };

    /* Getter/Setter methods */
    void setAPN(char* apn, unsigned int len);
    inline char* getAPN() const { return (char*)mAPN; }
    inline uint32_t getAPNLen() const { return mAPNLen; }
    inline void setBearer(AGpsBearerType bearer) { mBearer = bearer; }
    inline LocApnTypeMask getApnTypeMask() const { return mApnTypeMask; }
    inline void setApnTypeMask(LocApnTypeMask apnTypeMask)
    { mApnTypeMask = apnTypeMask; }
    inline AGpsBearerType getBearer() const { return mBearer; }
    inline void setType(AGpsExtType type) { mAgpsType = type; }
    inline AGpsExtType getType() const { return mAgpsType; }
    inline void setCurrentSubscriber(AgpsSubscriber* subscriber)
    { mCurrentSubscriber = subscriber; }

    inline void registerFrameworkStatusCallback(AgnssStatusIpV4Cb frameworkStatusV4Cb) {
        mFrameworkStatusV4Cb = frameworkStatusV4Cb;
    }

    /* Fetch subscriber with specified handle */
    AgpsSubscriber* getSubscriber(int connHandle);

    /* Fetch first active or inactive subscriber in list
     * isInactive = true : fetch first inactive subscriber
     * isInactive = false : fetch first active subscriber */
    AgpsSubscriber* getFirstSubscriber(bool isInactive);

    /* Process LOC AGPS Event being passed in
     * onRsrcEvent */
    virtual void processAgpsEvent(AgpsEvent event);

    /* Drop all subscribers, in case of Modem SSR */
    void dropAllSubscribers();

protected:
    /* Remove the specified subscriber from list if present.
     * Also delete the subscriber instance. */
    void deleteSubscriber(AgpsSubscriber* subscriber);

private:
    /* Send call setup request to framework
     * sendRsrcRequest(LOC_GPS_REQUEST_AGPS_DATA_CONN)
     * sendRsrcRequest(LOC_GPS_RELEASE_AGPS_DATA_CONN) */
    void requestOrReleaseDataConn(bool request);

    /* Individual event processing methods */
    void processAgpsEventSubscribe();
    void processAgpsEventUnsubscribe();
    void processAgpsEventGranted();
    void processAgpsEventReleased();
    void processAgpsEventDenied();

    /* Clone the passed in subscriber and add to the subscriber list
     * if not already present */
    void addSubscriber(AgpsSubscriber* subscriber);

    /* Notify subscribers about AGPS events */
    void notifyAllSubscribers(
            AgpsEvent event, bool deleteSubscriberPostNotify,
            AgpsNotificationType notificationType);
    virtual void notifyEventToSubscriber(
            AgpsEvent event, AgpsSubscriber* subscriber,
            bool deleteSubscriberPostNotify);

    /* Do we have any subscribers in active state */
    bool anyActiveSubscribers();

    /* Transition state */
    void transitionState(AgpsState newState);
};

/* LOC AGPS MANAGER */
class AgpsManager {

    friend class AgpsStateMachine;
public:
    /* CONSTRUCTOR */
    AgpsManager():
        mAtlOpenStatusCb(), mAtlCloseStatusCb(),
        mAgnssNif(NULL), mInternetNif(NULL)/*, mDsNif(NULL)*/ {}

    /* Register callbacks */
    inline void registerATLCallbacks(AgpsAtlOpenStatusCb  atlOpenStatusCb,
            AgpsAtlCloseStatusCb                atlCloseStatusCb) {

        mAtlOpenStatusCb = atlOpenStatusCb;
        mAtlCloseStatusCb = atlCloseStatusCb;
    }

    /* Check if AGPS client is registered */
    inline bool isRegistered() { return nullptr != mAgnssNif || nullptr != mInternetNif; }

    /* Create all AGPS state machines */
    void createAgpsStateMachines(const AgpsCbInfo& cbInfo);

    /* Process incoming ATL requests */
    void requestATL(int connHandle, AGpsExtType agpsType, LocApnTypeMask apnTypeMask);
    void releaseATL(int connHandle);
    /* Process incoming framework data call events */
    void reportAtlOpenSuccess(AGpsExtType agpsType, char* apnName, int apnLen,
            AGpsBearerType bearerType);
    void reportAtlOpenFailed(AGpsExtType agpsType);
    void reportAtlClosed(AGpsExtType agpsType);

    /* Handle Modem SSR */
    void handleModemSSR();

protected:

    AgpsAtlOpenStatusCb   mAtlOpenStatusCb;
    AgpsAtlCloseStatusCb  mAtlCloseStatusCb;
    AgpsStateMachine*   mAgnssNif;
    AgpsStateMachine*   mInternetNif;
private:
    /* Fetch state machine for handling request ATL call */
    AgpsStateMachine* getAgpsStateMachine(AGpsExtType agpsType);
};

/* Request SUPL/INTERNET/SUPL_ES ATL
 * This LocMsg is defined in this header since it has to be used from more
 * than one place, other Agps LocMsg are restricted to GnssAdapter and
 * declared inline */
struct AgpsMsgRequestATL: public LocMsg {

    AgpsManager* mAgpsManager;
    int mConnHandle;
    AGpsExtType mAgpsType;
    LocApnTypeMask mApnTypeMask;

    inline AgpsMsgRequestATL(AgpsManager* agpsManager, int connHandle,
            AGpsExtType agpsType, LocApnTypeMask apnTypeMask) :
            LocMsg(), mAgpsManager(agpsManager), mConnHandle(connHandle),
            mAgpsType(agpsType), mApnTypeMask(apnTypeMask){

        LOC_LOGV("AgpsMsgRequestATL");
    }

    inline virtual void proc() const {

        LOC_LOGV("AgpsMsgRequestATL::proc()");
        mAgpsManager->requestATL(mConnHandle, mAgpsType, mApnTypeMask);
    }
};

namespace AgpsUtils {

AGpsBearerType ipTypeToBearerType(LocApnIpType ipType);
LocApnIpType bearerTypeToIpType(AGpsBearerType bearerType);

}

#endif /* AGPS_H */
