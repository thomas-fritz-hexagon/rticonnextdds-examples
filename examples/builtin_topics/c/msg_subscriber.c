/*******************************************************************************
 (c) 2005-2014 Copyright, Real-Time Innovations, Inc.  All rights reserved.
 RTI grants Licensee a license to use, modify, compile, and create derivative
 works of the Software.  Licensee has the right to distribute object form only
 for use with RTI products.  The Software is provided "as is", with no warranty
 of any type, including any warranty for fitness for any purpose. RTI is under
 no obligation to maintain or support the Software.  RTI shall not be liable for
 any incidental or consequential damages arising out of the use or inability to
 use the software.
 ******************************************************************************/
/* msg_subscriber.c

   A subscription example

   This file is derived from code automatically generated by the rtiddsgen
   command:

   rtiddsgen -language C -example <arch> msg.idl

   Example subscription of type msg automatically generated by
   'rtiddsgen'. To test them follow these steps:

   (1) Compile this file and the example publication.

   (2) Start the subscription on the same domain used for RTI Data Distribution
       Service  with the command
       objs/<arch>/msg_subscriber <domain_id> <sample_count>

   (3) Start the publication on the same domain used for RTI Data Distribution
       Service with the command
       objs/<arch>/msg_publisher <domain_id> <sample_count>

   (4) [Optional] Specify the list of discovery initial peers and
       multicast receive addresses via an environment variable or a file
       (in the current working directory) called NDDS_DISCOVERY_PEERS.

   You can run any number of publishers and subscribers programs, and can
   add and remove them dynamically from the domain.


   Example:

       To run the example application on domain <domain_id>:

       On Unix:

       objs/<arch>/msg_publisher <domain_id>
       objs/<arch>/msg_subscriber <domain_id>

       On Windows:

       objs\<arch>\msg_publisher <domain_id>
       objs\<arch>\msg_subscriber <domain_id>


modification history
------------ -------
* Set user_data QoS fields in participant and datareader with
  strings given on command line

* Do not ignore data readers since it is a bad security practice.

* Fix null char in participant user_data field.
*/

#include <stdio.h>
#include <stdlib.h>
#include "ndds/ndds_c.h"
#include "msg.h"
#include "msgSupport.h"

void msgListener_on_requested_deadline_missed(void* listener_data,
        DDS_DataReader* reader,
        const struct DDS_RequestedDeadlineMissedStatus *status) {
}

void msgListener_on_requested_incompatible_qos(void* listener_data,
        DDS_DataReader* reader,
        const struct DDS_RequestedIncompatibleQosStatus *status) {

}

void msgListener_on_sample_rejected(void* listener_data, DDS_DataReader* reader,
        const struct DDS_SampleRejectedStatus *status) {
}

void msgListener_on_liveliness_changed(void* listener_data,
        DDS_DataReader* reader,
        const struct DDS_LivelinessChangedStatus *status) {
}

void msgListener_on_sample_lost(void* listener_data, DDS_DataReader* reader,
        const struct DDS_SampleLostStatus *status) {
}

void msgListener_on_subscription_matched(void* listener_data,
        DDS_DataReader* reader,
        const struct DDS_SubscriptionMatchedStatus *status) {
}

void msgListener_on_data_available(void* listener_data,
        DDS_DataReader* reader) {
    msgDataReader *msg_reader = NULL;
    struct msgSeq data_seq = DDS_SEQUENCE_INITIALIZER;
    struct DDS_SampleInfoSeq info_seq = DDS_SEQUENCE_INITIALIZER;
    DDS_ReturnCode_t retcode;
    int i;

    msg_reader = msgDataReader_narrow(reader);
    if (msg_reader == NULL) {
        printf("DataReader narrow error\n");
        return;
    }

    retcode = msgDataReader_take(msg_reader, &data_seq, &info_seq,
            DDS_LENGTH_UNLIMITED, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
            DDS_ANY_INSTANCE_STATE);
    if (retcode == DDS_RETCODE_NO_DATA) {
        return;
    } else if (retcode != DDS_RETCODE_OK) {
        printf("take error %d\n", retcode);
        return;
    }

    for (i = 0; i < msgSeq_get_length(&data_seq); ++i) {
        if (DDS_SampleInfoSeq_get_reference(&info_seq, i)->valid_data) {
            msgTypeSupport_print_data(msgSeq_get_reference(&data_seq, i));
        }
    }

    retcode = msgDataReader_return_loan(msg_reader, &data_seq, &info_seq);
    if (retcode != DDS_RETCODE_OK) {
        printf("return loan error %d\n", retcode);
    }
}

/* Delete all entities */
static int subscriber_shutdown(DDS_DomainParticipant *participant) {
    DDS_ReturnCode_t retcode;
    int status = 0;

    if (participant != NULL) {
        retcode = DDS_DomainParticipant_delete_contained_entities(participant);
        if (retcode != DDS_RETCODE_OK) {
            printf("delete_contained_entities error %d\n", retcode);
            status = -1;
        }

        retcode = DDS_DomainParticipantFactory_delete_participant(
                DDS_TheParticipantFactory, participant);
        if (retcode != DDS_RETCODE_OK) {
            printf("delete_participant error %d\n", retcode);
            status = -1;
        }
    }

    /* RTI Data Distribution Service provides the finalize_instance() method on
       domain participant factory for users who want to release memory used
       by the participant factory. Uncomment the following block of code for
       clean destruction of the singleton. */
    /*
    retcode = DDS_DomainParticipantFactory_finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        printf("finalize_instance error %d\n", retcode);
        status = -1;
    }
    */

    return status;
}

static int subscriber_main(int domain_id, int sample_count,
        char *participant_auth) {
    DDS_DomainParticipant *participant = NULL;
    DDS_Subscriber *subscriber = NULL;
    DDS_Topic *topic = NULL;
    struct DDS_DataReaderListener reader_listener =
            DDS_DataReaderListener_INITIALIZER;
    DDS_DataReader *reader = NULL;
    DDS_ReturnCode_t retcode;
    const char *type_name = NULL;
    int count = 0;
    struct DDS_Duration_t poll_period = { 1, 0 };
    struct DDS_DomainParticipantQos participant_qos =
            DDS_DomainParticipantQos_INITIALIZER;
    struct DDS_DataReaderQos datareader_qos = DDS_DataReaderQos_INITIALIZER;
    int len, max;

    retcode = DDS_DomainParticipantFactory_get_default_participant_qos(
            DDS_TheParticipantFactory, &participant_qos);
    if (retcode != DDS_RETCODE_OK) {
        printf("get_default_participant_qos error\n");
        return -1;
    }

    /* We include the subscriber credentials into de USER_DATA QoS. */
    len = strlen(participant_auth) + 1;
    max = participant_qos.resource_limits.participant_user_data_max_length;

    if (len > max) {
        printf("error, participant user_data exceeds resource limits\n");
    } else {
        /*
         *  DDS_Octet is defined to be 8 bits.  If chars are not 8 bits
         *  on your system, this will not work.
         */
        DDS_OctetSeq_from_array(&participant_qos.user_data.value,
                (DDS_Octet*) (participant_auth), len);
    }

    /* To customize participant QoS, use
       the configuration file USER_QOS_PROFILES.xml */
    participant = DDS_DomainParticipantFactory_create_participant(
            DDS_TheParticipantFactory, domain_id, &participant_qos,
            NULL /* listener */, DDS_STATUS_MASK_NONE);
    if (participant == NULL) {
        printf("create_participant error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Done!  All the listeners are installed, so we can enable the
     * participant now.
     */
    if (DDS_Entity_enable((DDS_Entity*) participant) != DDS_RETCODE_OK) {
        printf("***Error: Failed to Enable Participant\n");
        return -1;
    }

    /* To customize subscriber QoS, use
       the configuration file USER_QOS_PROFILES.xml */
    subscriber = DDS_DomainParticipant_create_subscriber(participant,
            &DDS_SUBSCRIBER_QOS_DEFAULT, NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (subscriber == NULL) {
        printf("create_subscriber error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Register the type before creating the topic */
    type_name = msgTypeSupport_get_type_name();
    retcode = msgTypeSupport_register_type(participant, type_name);
    if (retcode != DDS_RETCODE_OK) {
        printf("register_type error %d\n", retcode);
        subscriber_shutdown(participant);
        return -1;
    }

    /* To customize topic QoS, use
       the configuration file USER_QOS_PROFILES.xml */
    topic = DDS_DomainParticipant_create_topic(participant, "Example msg",
            type_name, &DDS_TOPIC_QOS_DEFAULT, NULL /* listener */,
            DDS_STATUS_MASK_NONE);
    if (topic == NULL) {
        printf("create_topic error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Set up a data reader listener */
    reader_listener.on_requested_deadline_missed =
            msgListener_on_requested_deadline_missed;
    reader_listener.on_requested_incompatible_qos =
            msgListener_on_requested_incompatible_qos;
    reader_listener.on_sample_rejected = msgListener_on_sample_rejected;
    reader_listener.on_liveliness_changed = msgListener_on_liveliness_changed;
    reader_listener.on_sample_lost = msgListener_on_sample_lost;
    reader_listener.on_subscription_matched =
            msgListener_on_subscription_matched;
    reader_listener.on_data_available = msgListener_on_data_available;

    /* To customize data reader QoS, use
       the configuration file USER_QOS_PROFILES.xml */
    reader = DDS_Subscriber_create_datareader(subscriber,
            DDS_Topic_as_topicdescription(topic), &DDS_DATAREADER_QOS_DEFAULT,
            &reader_listener, DDS_STATUS_MASK_ALL);
    if (reader == NULL) {
        printf("create_datareader error\n");
        subscriber_shutdown(participant);
        return -1;
    }

    /* Main loop */
    for (count = 0; (sample_count == 0) || (count < sample_count); ++count) {
        /*
         printf("msg subscriber sleeping for %d sec...\n",
         poll_period.sec);
         */
        NDDS_Utility_sleep(&poll_period);
    }

    /* Cleanup and delete all entities */
    return subscriber_shutdown(participant);
}

int main(int argc, char *argv[]) {
    int domain_id = 0;
    int sample_count = 0; /* infinite loop */

    /*
     * Changes for Builtin_Topics
     * Get arguments for auth strings and pass to subscriber_main()
     */

    char *participant_auth = "password";

    if (argc >= 2) {
        domain_id = atoi(argv[1]);
    }
    if (argc >= 3) {
        sample_count = atoi(argv[2]);
    }
    if (argc >= 4) {
        participant_auth = argv[3];
    }

    /* Uncomment this to turn on additional logging
    NDDS_Config_Logger_set_verbosity_by_category(
        NDDS_Config_Logger_get_instance(),
        NDDS_CONFIG_LOG_CATEGORY_API,
        NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
    */

    subscriber_main(domain_id, sample_count, participant_auth);
    return 0;
}
