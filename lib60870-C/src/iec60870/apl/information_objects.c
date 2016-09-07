/*
 *  Copyright 2016 MZ Automation GmbH
 *
 *  This file is part of lib60870-C
 *
 *  lib60870-C is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870-C is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870-C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "iec60870_common.h"
#include "apl_types_internal.h"
#include "information_objects.h"
#include "information_objects_internal.h"
#include "lib_memory.h"
#include "frame.h"
#include "platform_endian.h"

typedef struct sInformationObjectVFT* InformationObjectVFT;


typedef void (*EncodeFunction)(InformationObject self, Frame frame, ConnectionParameters parameters);
typedef void (*DestroyFunction)(InformationObject self);

struct sInformationObjectVFT {
    EncodeFunction encode;
    DestroyFunction destroy;
#if 0
    const char* (*toString)(InformationObject self);
#endif
};


/*****************************************
 * Basic data types
 ****************************************/

void
SingleEvent_setEventState(SingleEvent self, EventState eventState)
{
    uint8_t value = *self;

    value &= 0xfc;

    value += eventState;

    *self = value;
}

EventState
SingleEvent_getEventState(SingleEvent self)
{
    return (EventState) (*self & 0x3);
}

void
SingleEvent_setQDP(SingleEvent self, QualityDescriptorP qdp)
{
    uint8_t value = *self;

    value &= 0x03;

    value += qdp;

    *self = value;
}

QualityDescriptorP
SingleEvent_getQDP(SingleEvent self)
{
    return (QualityDescriptor) (*self & 0xfc);
}

/*****************************************
 * Information object hierarchy
 *****************************************/

/*****************************************
 * InformationObject (base class)
 *****************************************/

struct sInformationObject {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;
};

void
InformationObject_encode(InformationObject self, Frame frame, ConnectionParameters parameters)
{
    self->virtualFunctionTable->encode(self, frame, parameters);
}

void
InformationObject_destroy(InformationObject self)
{
    self->virtualFunctionTable->destroy(self);
}

int
InformationObject_getObjectAddress(InformationObject self)
{
    return self->objectAddress;
}


static void
InformationObject_encodeBase(InformationObject self, Frame frame, ConnectionParameters parameters)
{
    Frame_setNextByte(frame, (uint8_t)(self->objectAddress & 0xff));

    if (parameters->sizeOfIOA > 1)
        Frame_setNextByte(frame, (uint8_t)((self->objectAddress / 0x100) & 0xff));

    if (parameters->sizeOfIOA > 2)
        Frame_setNextByte(frame, (uint8_t)((self->objectAddress / 0x10000) & 0xff));
}

static void
InformationObject_getFromBuffer(InformationObject self, ConnectionParameters parameters,
        uint8_t* msg, int startIndex)
{
    /* parse information object address */
    self->objectAddress = msg [startIndex];

    if (parameters->sizeOfIOA > 1)
        self->objectAddress += (msg [startIndex + 1] * 0x100);

    if (parameters->sizeOfIOA > 2)
        self->objectAddress += (msg [startIndex + 2] * 0x10000);
}


/**********************************************
 * SinglePointInformation
 **********************************************/

struct sSinglePointInformation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;
};

static void
SinglePointInformation_encode(SinglePointInformation self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);
}

struct sInformationObjectVFT singlePointInformationVFT = {
        (EncodeFunction) SinglePointInformation_encode,
        (DestroyFunction) SinglePointInformation_destroy
};

static void
SinglePointInformation_initialize(SinglePointInformation self)
{
    self->virtualFunctionTable = &(singlePointInformationVFT);
    self->type = M_SP_NA_1;
}

SinglePointInformation
SinglePointInformation_create(SinglePointInformation self, int ioa, bool value,
        QualityDescriptor quality)
{
    if (self == NULL)
        self = (SinglePointInformation) GLOBAL_CALLOC(1, sizeof(struct sSinglePointInformation));

    if (self != NULL)
        SinglePointInformation_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;

    return self;
}

void
SinglePointInformation_destroy(SinglePointInformation self)
{
    GLOBAL_FREEMEM(self);
}

SinglePointInformation
SinglePointInformation_getFromBuffer(SinglePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (SinglePointInformation) GLOBAL_MALLOC(sizeof(struct sSinglePointInformation));

        if (self != NULL)
            SinglePointInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);
    }

    return self;
}

bool
SinglePointInformation_getValue(SinglePointInformation self)
{
    return self->value;
}

QualityDescriptor
SinglePointInformation_getQuality(SinglePointInformation self)
{
    return self->quality;
}


/**********************************************
 * StepPositionInformation
 **********************************************/

struct sStepPositionInformation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;
};

static void
StepPositionInformation_encode(StepPositionInformation self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT stepPositionInformationVFT = {
        (EncodeFunction) StepPositionInformation_encode,
        (DestroyFunction) StepPositionInformation_destroy
};

static void
StepPositionInformation_initialize(StepPositionInformation self)
{
    self->virtualFunctionTable = &(stepPositionInformationVFT);
    self->type = M_ST_NA_1;
}

StepPositionInformation
StepPositionInformation_create(StepPositionInformation self, int ioa, int value, bool isTransient,
        QualityDescriptor quality)
{
    if (self == NULL)
        self = (StepPositionInformation) GLOBAL_CALLOC(1, sizeof(struct sStepPositionInformation));

    if (self != NULL)
        StepPositionInformation_initialize(self);

    self->objectAddress = ioa;

    if (value > 63)
        value = 63;
    else if (value < -64)
        value = -64;

    if (value < 0)
        value = value + 128;

    self->vti = value;

    if (isTransient)
        self->vti |= 0x80;

    self->quality = quality;

    return self;
}

void
StepPositionInformation_destroy(StepPositionInformation self)
{
    GLOBAL_FREEMEM(self);
}

int
StepPositionInformation_getObjectAddress(StepPositionInformation self)
{
    return self->objectAddress;
}

int
StepPositionInformation_getValue(StepPositionInformation self)
{
    int value = (self->vti & 0x7f);

    if (value > 63)
        value = value - 128;

    return value;
}

bool
StepPositionInformation_isTransient(StepPositionInformation self)
{
    return ((self->vti & 0x80) == 0x80);
}

QualityDescriptor
StepPositionInformation_getQuality(StepPositionInformation self)
{
    return self->quality;
}


StepPositionInformation
StepPositionInformation_getFromBuffer(StepPositionInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (StepPositionInformation) GLOBAL_MALLOC(sizeof(struct sStepPositionInformation));

        if (self != NULL)
            StepPositionInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex];
    }

    return self;
}


/**********************************************
 * StepPositionWithCP56Time2a
 **********************************************/

struct sStepPositionWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
StepPositionWithCP56Time2a_encode(StepPositionWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT stepPositionWithCP56Time2aVFT = {
        (EncodeFunction) StepPositionWithCP56Time2a_encode,
        (DestroyFunction) StepPositionWithCP56Time2a_destroy
};

static void
StepPositionWithCP56Time2a_initialize(StepPositionWithCP56Time2a self)
{
    self->virtualFunctionTable = &(stepPositionWithCP56Time2aVFT);
    self->type = M_ST_TB_1;
}

void
StepPositionWithCP56Time2a_destroy(StepPositionWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_create(StepPositionWithCP56Time2a self, int ioa, int value, bool isTransient,
        QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (StepPositionWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sStepPositionWithCP56Time2a));

    if (self != NULL)
        StepPositionWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;

    if (value > 63)
        value = 63;
    else if (value < -64)
        value = -64;

    if (value < 0)
        value = value + 128;

    self->vti = value;

    if (isTransient)
        self->vti |= 0x80;

    self->quality = quality;

    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
StepPositionWithCP56Time2a_getTimestamp(StepPositionWithCP56Time2a self)
{
    return &(self->timestamp);
}

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_getFromBuffer(StepPositionWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (StepPositionWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sStepPositionWithCP56Time2a));

        if (self != NULL)
            StepPositionWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/**********************************************
 * StepPositionWithCP24Time2a
 **********************************************/

struct sStepPositionWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};


static void
StepPositionWithCP24Time2a_encode(StepPositionWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT stepPositionWithCP24Time2aVFT = {
        (EncodeFunction) StepPositionWithCP24Time2a_encode,
        (DestroyFunction) StepPositionWithCP24Time2a_destroy
};

static void
StepPositionWithCP24Time2a_initialize(StepPositionWithCP24Time2a self)
{
    self->virtualFunctionTable = &(stepPositionWithCP24Time2aVFT);
    self->type = M_ST_TA_1;
}

void
StepPositionWithCP24Time2a_destroy(StepPositionWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_create(StepPositionWithCP24Time2a self, int ioa, int value, bool isTransient,
        QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (StepPositionWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sStepPositionWithCP24Time2a));

    if (self != NULL)
        StepPositionWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;

    if (value > 63)
        value = 63;
    else if (value < -64)
        value = -64;

    if (value < 0)
        value = value + 128;

    self->vti = value;

    if (isTransient)
        self->vti |= 0x80;

    self->quality = quality;

    self->timestamp = *timestamp;

    return self;
}


CP24Time2a
StepPositionWithCP24Time2a_getTimestamp(StepPositionWithCP24Time2a self)
{
    return &(self->timestamp);
}

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_getFromBuffer(StepPositionWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (StepPositionWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sStepPositionWithCP24Time2a));

        if (self != NULL)
            StepPositionWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/**********************************************
 * DoublePointInformation
 **********************************************/

struct sDoublePointInformation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;
};

static void
DoublePointInformation_encode(DoublePointInformation self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);
}

struct sInformationObjectVFT doublePointInformationVFT = {
        (EncodeFunction) DoublePointInformation_encode,
        (DestroyFunction) DoublePointInformation_destroy
};

void
DoublePointInformation_destroy(DoublePointInformation self)
{
    GLOBAL_FREEMEM(self);
}

static void
DoublePointInformation_initialize(DoublePointInformation self)
{
    self->virtualFunctionTable = &(doublePointInformationVFT);
    self->type = M_DP_NA_1;
}

DoublePointInformation
DoublePointInformation_create(DoublePointInformation self, int ioa, DoublePointValue value,
        QualityDescriptor quality)
{
    if (self == NULL)
        self = (DoublePointInformation) GLOBAL_CALLOC(1, sizeof(struct sDoublePointInformation));

    if (self != NULL)
        DoublePointInformation_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;

    return self;
}

DoublePointValue
DoublePointInformation_getValue(DoublePointInformation self)
{
    return self->value;
}

QualityDescriptor
DoublePointInformation_getQuality(DoublePointInformation self)
{
    return self->quality;
}

DoublePointInformation
DoublePointInformation_getFromBuffer(DoublePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (DoublePointInformation) GLOBAL_MALLOC(sizeof(struct sDoublePointInformation));

        if (self != NULL)
            DoublePointInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = (DoublePointValue) ((siq & 0x01) == 0x03);

        self->quality = (QualityDescriptor) (siq & 0xf0);
    }

    return self;
}


/*******************************************
 * DoublePointWithCP24Time2a
 *******************************************/

struct sDoublePointWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
DoublePointWithCP24Time2a_encode(DoublePointWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}


struct sInformationObjectVFT doublePointWithCP24Time2aVFT = {
        (EncodeFunction) DoublePointWithCP24Time2a_encode,
        (DestroyFunction) DoublePointWithCP24Time2a_destroy
};

void
DoublePointWithCP24Time2a_destroy(DoublePointWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
DoublePointWithCP24Time2a_initialize(DoublePointWithCP24Time2a self)
{
    self->virtualFunctionTable = &(doublePointWithCP24Time2aVFT);
    self->type = M_DP_TA_1;
}

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_create(DoublePointWithCP24Time2a self, int ioa, DoublePointValue value,
        QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (DoublePointWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sDoublePointWithCP24Time2a));

    if (self != NULL)
        DoublePointWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
DoublePointWithCP24Time2a_getTimestamp(DoublePointWithCP24Time2a self)
{
    return &(self->timestamp);
}

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_getFromBuffer(DoublePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (DoublePointWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sDoublePointWithCP24Time2a));

        if (self != NULL)
            DoublePointWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = (DoublePointValue) ((siq & 0x01) == 0x03);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * DoublePointWithCP56Time2a
 *******************************************/

struct sDoublePointWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
DoublePointWithCP56Time2a_encode(DoublePointWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}



struct sInformationObjectVFT doublePointWithCP56Time2aVFT = {
        (EncodeFunction) DoublePointWithCP56Time2a_encode,
        (DestroyFunction) DoublePointWithCP56Time2a_destroy
};

void
DoublePointWithCP56Time2a_destroy(DoublePointWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
DoublePointWithCP56Time2a_initialize(DoublePointWithCP56Time2a self)
{
    self->virtualFunctionTable = &(doublePointWithCP56Time2aVFT);
    self->type = M_DP_TB_1;
}

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_create(DoublePointWithCP56Time2a self, int ioa, DoublePointValue value,
        QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (DoublePointWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sDoublePointWithCP56Time2a));

    if (self != NULL)
        DoublePointWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}


CP56Time2a
DoublePointWithCP56Time2a_getTimestamp(DoublePointWithCP56Time2a self)
{
    return &(self->timestamp);
}

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_getFromBuffer(DoublePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (DoublePointWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sDoublePointWithCP56Time2a));

        if (self != NULL)
            DoublePointWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = (DoublePointValue) ((siq & 0x01) == 0x03);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * SinglePointWithCP24Time2a
 *******************************************/

struct sSinglePointWithCP24Time2a {
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};


static void
SinglePointWithCP24Time2a_encode(SinglePointWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT singlePointWithCP24Time2aVFT = {
        (EncodeFunction) SinglePointWithCP24Time2a_encode,
        (DestroyFunction) SinglePointWithCP24Time2a_destroy
};

void
SinglePointWithCP24Time2a_destroy(SinglePointWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
SinglePointWithCP24Time2a_initialize(SinglePointWithCP24Time2a self)
{
    self->virtualFunctionTable = &(singlePointWithCP24Time2aVFT);
    self->type = M_SP_TA_1;
}

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_create(SinglePointWithCP24Time2a self, int ioa, bool value,
        QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (SinglePointWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sSinglePointWithCP24Time2a));

    if (self != NULL)
        SinglePointWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
SinglePointWithCP24Time2a_getTimestamp(SinglePointWithCP24Time2a self)
{
    return &(self->timestamp);
}

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_getFromBuffer(SinglePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (SinglePointWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sSinglePointWithCP24Time2a));

        if (self != NULL)
            SinglePointWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}



/*******************************************
 * SinglePointWithCP56Time2a
 *******************************************/

struct sSinglePointWithCP56Time2a {
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};


static void
SinglePointWithCP56Time2a_encode(SinglePointWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}


struct sInformationObjectVFT singlePointWithCP56Time2aVFT = {
        (EncodeFunction) SinglePointWithCP56Time2a_encode,
        (DestroyFunction) SinglePointWithCP56Time2a_destroy
};

static void
SinglePointWithCP56Time2a_initialize(SinglePointWithCP56Time2a self)
{
    self->virtualFunctionTable = &(singlePointWithCP56Time2aVFT);
    self->type = M_SP_TB_1;
}

SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_create(SinglePointWithCP56Time2a self, int ioa, bool value,
        QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (SinglePointWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sSinglePointWithCP56Time2a));

    if (self != NULL)
        SinglePointWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

void
SinglePointWithCP56Time2a_destroy(SinglePointWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

CP56Time2a
SinglePointWithCP56Time2a_getTimestamp(SinglePointWithCP56Time2a self)
{
    return &(self->timestamp);
}


SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_getFromBuffer(SinglePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (SinglePointWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSinglePointWithCP56Time2a));

        if (self != NULL)
            SinglePointWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}



/**********************************************
 * BitString32
 **********************************************/

struct sBitString32 {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;
};

static void
BitString32_encode(BitString32 self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT bitString32VFT = {
        (EncodeFunction) BitString32_encode,
        (DestroyFunction) BitString32_destroy
};

static void
BitString32_initialize(BitString32 self)
{
    self->virtualFunctionTable = &(bitString32VFT);
    self->type = M_BO_NA_1;
}

void
BitString32_destroy(BitString32 self)
{
    GLOBAL_FREEMEM(self);
}

BitString32
BitString32_create(BitString32 self, int ioa, uint32_t value)
{
    if (self == NULL)
         self = (BitString32) GLOBAL_CALLOC(1, sizeof(struct sBitString32));

    if (self != NULL)
        BitString32_initialize(self);

    self->objectAddress = ioa;
    self->value = value;

    return self;
}

uint32_t
BitString32_getValue(BitString32 self)
{
    return self->value;
}

QualityDescriptor
BitString32_getQuality(BitString32 self)
{
    return self->quality;
}

BitString32
BitString32_getFromBuffer(BitString32 self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (BitString32) GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            BitString32_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/**********************************************
 * Bitstring32WithCP24Time2a
 **********************************************/

struct sBitstring32WithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
Bitstring32WithCP24Time2a_encode(Bitstring32WithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT bitstring32WithCP24Time2aVFT = {
        (EncodeFunction) Bitstring32WithCP24Time2a_encode,
        (DestroyFunction) Bitstring32WithCP24Time2a_destroy
};

static void
Bitstring32WithCP24Time2a_initialize(Bitstring32WithCP24Time2a self)
{
    self->virtualFunctionTable = &(bitstring32WithCP24Time2aVFT);
    self->type = M_BO_TA_1;
}

void
Bitstring32WithCP24Time2a_destroy(Bitstring32WithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_create(Bitstring32WithCP24Time2a self, int ioa, uint32_t value, CP24Time2a timestamp)
{
    if (self == NULL)
         self = (Bitstring32WithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sBitstring32WithCP24Time2a));

    if (self != NULL)
        Bitstring32WithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
Bitstring32WithCP24Time2a_getTimestamp(Bitstring32WithCP24Time2a self)
{
    return &(self->timestamp);
}

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_getFromBuffer(Bitstring32WithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (Bitstring32WithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            Bitstring32WithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/**********************************************
 * Bitstring32WithCP56Time2a
 **********************************************/

struct sBitstring32WithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
Bitstring32WithCP56Time2a_encode(Bitstring32WithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT bitstring32WithCP56Time2aVFT = {
        (EncodeFunction) Bitstring32WithCP56Time2a_encode,
        (DestroyFunction) Bitstring32WithCP56Time2a_destroy
};

static void
Bitstring32WithCP56Time2a_initialize(Bitstring32WithCP56Time2a self)
{
    self->virtualFunctionTable = &(bitstring32WithCP56Time2aVFT);
    self->type = M_BO_TB_1;
}

void
Bitstring32WithCP56Time2a_destroy(Bitstring32WithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_create(Bitstring32WithCP56Time2a self, int ioa, uint32_t value, CP56Time2a timestamp)
{
    if (self == NULL)
         self = (Bitstring32WithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sBitstring32WithCP56Time2a));

    if (self != NULL)
        Bitstring32WithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->timestamp = *timestamp;

    return self;
}


CP56Time2a
Bitstring32WithCP56Time2a_getTimestamp(Bitstring32WithCP56Time2a self)
{
    return &(self->timestamp);
}

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_getFromBuffer(Bitstring32WithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (Bitstring32WithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            Bitstring32WithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/**********************************************
 * MeasuredValueNormalized
 **********************************************/

struct sMeasuredValueNormalized {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

static int
getScaledValue(uint8_t* encodedValue)
{
    int value;

    value = encodedValue[0];
    value += (encodedValue[1] * 0x100);

    if (value > 32767)
        value = value - 65536;

    return value;
}

static void
setScaledValue(uint8_t* encodedValue, int value)
{
    int valueToEncode;

    if (value < 0)
        valueToEncode = value + 65536;
    else
        valueToEncode = value;

    encodedValue[0] = (uint8_t) (valueToEncode % 256);
    encodedValue[1] = (uint8_t) (valueToEncode / 256);
}

static void
MeasuredValueNormalized_encode(MeasuredValueNormalized self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->encodedValue[0]);
    Frame_setNextByte(frame, self->encodedValue[1]);

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT measuredValueNormalizedVFT = {
        (EncodeFunction) MeasuredValueNormalized_encode,
        (DestroyFunction) MeasuredValueNormalized_destroy
};

static void
MeasuredValueNormalized_initialize(MeasuredValueNormalized self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedVFT);
    self->type = M_ME_NA_1;
}

void
MeasuredValueNormalized_destroy(MeasuredValueNormalized self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalized
MeasuredValueNormalized_create(MeasuredValueNormalized self, int ioa, float value, QualityDescriptor quality)
{
    if (self == NULL)
        self = (MeasuredValueNormalized) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalized));

    if (self != NULL)
        MeasuredValueNormalized_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalized_setValue(self, value);

    self->quality = quality;

    return self;
}

float
MeasuredValueNormalized_getValue(MeasuredValueNormalized self)
{
    float nv = (float) (getScaledValue(self->encodedValue)) / 32767.f;

    return nv;
}

void
MeasuredValueNormalized_setValue(MeasuredValueNormalized self, float value)
{
    if (value > 1.0f)
        value = 1.0f;
    else if (value < -1.0f)
        value = -1.0f;

    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, scaledValue);
}

QualityDescriptor
MeasuredValueNormalized_getQuality(MeasuredValueNormalized self)
{
    return self->quality;
}

MeasuredValueNormalized
MeasuredValueNormalized_getFromBuffer(MeasuredValueNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueNormalized) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalized));

        if (self != NULL)
            MeasuredValueNormalized_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/***********************************************************************
 * MeasuredValueNormalizedWithCP24Time2a : MeasuredValueNormalized
 ***********************************************************************/

struct sMeasuredValueNormalizedWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
MeasuredValueNormalizedWithCP24Time2a_encode(MeasuredValueNormalizedWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT measuredValueNormalizedWithCP24Time2aVFT = {
        (EncodeFunction) MeasuredValueNormalizedWithCP24Time2a_encode,
        (DestroyFunction) MeasuredValueNormalizedWithCP24Time2a_destroy
};

static void
MeasuredValueNormalizedWithCP24Time2a_initialize(MeasuredValueNormalizedWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithCP24Time2aVFT);
    self->type = M_ME_TA_1;
}

void
MeasuredValueNormalizedWithCP24Time2a_destroy(MeasuredValueNormalizedWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_create(MeasuredValueNormalizedWithCP24Time2a self, int ioa,
            float value, QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueNormalizedWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalizedWithCP24Time2a));

    if (self != NULL)
        MeasuredValueNormalizedWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalized_setValue((MeasuredValueNormalized) self, value);

    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}


CP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getTimestamp(MeasuredValueNormalizedWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueNormalizedWithCP24Time2a_setTimestamp(MeasuredValueNormalizedWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getFromBuffer(MeasuredValueNormalizedWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueNormalizedWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalizedWithCP24Time2a));

        if (self != NULL)
            MeasuredValueNormalizedWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***********************************************************************
 * MeasuredValueNormalizedWithCP56Time2a : MeasuredValueNormalized
 ***********************************************************************/

struct sMeasuredValueNormalizedWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
MeasuredValueNormalizedWithCP56Time2a_encode(MeasuredValueNormalizedWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT measuredValueNormalizedWithCP56Time2aVFT = {
        (EncodeFunction) MeasuredValueNormalizedWithCP56Time2a_encode,
        (DestroyFunction) MeasuredValueNormalizedWithCP56Time2a_destroy
};

static void
MeasuredValueNormalizedWithCP56Time2a_initialize(MeasuredValueNormalizedWithCP56Time2a self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithCP56Time2aVFT);
    self->type = M_ME_TD_1;
}

void
MeasuredValueNormalizedWithCP56Time2a_destroy(MeasuredValueNormalizedWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalizedWithCP56Time2a
MeasuredValueNormalizedWithCP56Time2a_create(MeasuredValueNormalizedWithCP56Time2a self, int ioa,
            float value, QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueNormalizedWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalizedWithCP56Time2a));

    if (self != NULL)
        MeasuredValueNormalizedWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalized_setValue((MeasuredValueNormalized) self, value);

    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}


CP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getTimestamp(MeasuredValueNormalizedWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueNormalizedWithCP56Time2a_setTimestamp(MeasuredValueNormalizedWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueNormalizedWithCP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getFromBuffer(MeasuredValueNormalizedWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueNormalizedWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalizedWithCP56Time2a));

        if (self != NULL)
            MeasuredValueNormalizedWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}



/*******************************************
 * MeasuredValueScaled
 *******************************************/

struct sMeasuredValueScaled {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

static void
MeasuredValueScaled_encode(MeasuredValueScaled self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);
}

struct sInformationObjectVFT measuredValueScaledVFT = {
        (EncodeFunction) MeasuredValueScaled_encode,
        (DestroyFunction) MeasuredValueScaled_destroy
};

static void
MeasuredValueScaled_initialize(MeasuredValueScaled self)
{
    self->virtualFunctionTable = &(measuredValueScaledVFT);
    self->type = M_ME_NB_1;
}

MeasuredValueScaled
MeasuredValueScaled_create(MeasuredValueScaled self, int ioa, int value, QualityDescriptor quality)
{
    if (self == NULL)
        self = (MeasuredValueScaled) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueScaled));

    if (self != NULL)
        MeasuredValueScaled_initialize(self);

    self->objectAddress = ioa;
    setScaledValue(self->encodedValue, value);
    self->quality = quality;

    return self;
}


void
MeasuredValueScaled_destroy(MeasuredValueScaled self)
{
    GLOBAL_FREEMEM(self);
}


int
MeasuredValueScaled_getValue(MeasuredValueScaled self)
{
    return getScaledValue(self->encodedValue);
}

void
MeasuredValueScaled_setValue(MeasuredValueScaled self, int value)
{
    //TODO check boundaries
    setScaledValue(self->encodedValue, value);
}

QualityDescriptor
MeasuredValueScaled_getQuality(MeasuredValueScaled self)
{
    return self->quality;
}

void
MeasuredValueScaled_setQuality(MeasuredValueScaled self, QualityDescriptor quality)
{
    self->quality = quality;
}

MeasuredValueScaled
MeasuredValueScaled_getFromBuffer(MeasuredValueScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueScaled) GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaled));

        if (self != NULL)
            MeasuredValueScaled_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/*******************************************
 * MeasuredValueScaledWithCP24Time2a
 *******************************************/

struct sMeasuredValueScaledWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
MeasuredValueScaledWithCP24Time2a_encode(MeasuredValueScaledWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT measuredValueScaledWithCP24Time2aVFT = {
        (EncodeFunction) MeasuredValueScaledWithCP24Time2a_encode,
        (DestroyFunction) MeasuredValueScaled_destroy
};

static void
MeasuredValueScaledWithCP24Time2a_initialize(MeasuredValueScaledWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueScaledWithCP24Time2aVFT);
    self->type = M_ME_TB_1;
}

void
MeasuredValueScaledWithCP24Time2a_destroy(MeasuredValueScaledWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueScaledWithCP24Time2a
MeasuredValueScaledWithCP24Time2a_create(MeasuredValueScaledWithCP24Time2a self, int ioa,
        int value, QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueScaledWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueScaledWithCP24Time2a));

    if (self != NULL)
        MeasuredValueScaledWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    setScaledValue(self->encodedValue, value);
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
MeasuredValueScaledWithCP24Time2a_getTimestamp(MeasuredValueScaledWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueScaledWithCP24Time2a_setTimestamp(MeasuredValueScaledWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueScaledWithCP24Time2a
MeasuredValueScaledWithCP24Time2a_getFromBuffer(MeasuredValueScaledWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueScaledWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaledWithCP24Time2a));

        if (self != NULL)
            MeasuredValueScaledWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueScaledWithCP56Time2a
 *******************************************/

struct sMeasuredValueScaledWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
MeasuredValueScaledWithCP56Time2a_encode(MeasuredValueScaledWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT measuredValueScaledWithCP56Time2aVFT = {
        (EncodeFunction) MeasuredValueScaledWithCP56Time2a_encode,
        (DestroyFunction) MeasuredValueScaledWithCP56Time2a_destroy
};

static void
MeasuredValueScaledWithCP56Time2a_initialize(MeasuredValueScaledWithCP56Time2a self)
{
    self->virtualFunctionTable = &measuredValueScaledWithCP56Time2aVFT;
    self->type = M_ME_TE_1;
}

void
MeasuredValueScaledWithCP56Time2a_destroy(MeasuredValueScaledWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_create(MeasuredValueScaledWithCP56Time2a self, int ioa,
        int value, QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueScaledWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueScaledWithCP56Time2a));

    if (self != NULL)
        MeasuredValueScaledWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    setScaledValue(self->encodedValue, value);
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
MeasuredValueScaledWithCP56Time2a_getTimestamp(MeasuredValueScaledWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueScaledWithCP56Time2a_setTimestamp(MeasuredValueScaledWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}


MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_getFromBuffer(MeasuredValueScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueScaledWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaledWithCP56Time2a));

        if (self != NULL)
            MeasuredValueScaledWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* scaled value */
        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueShort
 *******************************************/

struct sMeasuredValueShort {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;
};

static void
MeasuredValueShort_encode(MeasuredValueShort self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
    Frame_appendBytes(frame, valueBytes, 4);
#else
    Frame_setNextByte(frame, valueBytes[3]);
    Frame_setNextByte(frame, valueBytes[2]);
    Frame_setNextByte(frame, valueBytes[1]);
    Frame_setNextByte(frame, valueBytes[0]);
#endif

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT measuredValueShortVFT = {
        (EncodeFunction) MeasuredValueShort_encode,
        (DestroyFunction) MeasuredValueShort_destroy
};

static void
MeasuredValueShort_initialize(MeasuredValueShort self)
{
    self->virtualFunctionTable = &(measuredValueShortVFT);
    self->type = M_ME_NC_1;
}

void
MeasuredValueShort_destroy(MeasuredValueShort self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueShort
MeasuredValueShort_create(MeasuredValueShort self, int ioa, float value, QualityDescriptor quality)
{
    if (self == NULL)
        self = (MeasuredValueShort) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueShort));

    if (self != NULL)
        MeasuredValueShort_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;

    return self;
}

float
MeasuredValueShort_getValue(MeasuredValueShort self)
{
    return self->value;
}

void
MeasuredValueShort_setValue(MeasuredValueShort self, float value)
{
    self->value = value;
}

QualityDescriptor
MeasuredValueShort_getQuality(MeasuredValueShort self)
{
    return self->quality;
}

MeasuredValueShort
MeasuredValueShort_getFromBuffer(MeasuredValueShort self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueShort) GLOBAL_MALLOC(sizeof(struct sMeasuredValueShort));

        if (self != NULL)
            MeasuredValueShort_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/*******************************************
 * MeasuredValueFloatWithCP24Time2a
 *******************************************/

struct sMeasuredValueShortWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
MeasuredValueShortWithCP24Time2a_encode(MeasuredValueShortWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueShort_encode((MeasuredValueShort) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT measuredValueShortWithCP24Time2aVFT = {
        (EncodeFunction) MeasuredValueShortWithCP24Time2a_encode,
        (DestroyFunction) MeasuredValueShortWithCP24Time2a_destroy
};

static void
MeasuredValueShortWithCP24Time2a_initialize(MeasuredValueShortWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueShortWithCP24Time2aVFT);
    self->type = M_ME_TC_1;
}

void
MeasuredValueShortWithCP24Time2a_destroy(MeasuredValueShortWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueShortWithCP24Time2a
MeasuredValueShortWithCP24Time2a_create(MeasuredValueShortWithCP24Time2a self, int ioa,
        float value, QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueShortWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueShortWithCP24Time2a));

    if (self != NULL)
        MeasuredValueShortWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
MeasuredValueShortWithCP24Time2a_getTimestamp(MeasuredValueShortWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueShortWithCP24Time2a_setTimestamp(MeasuredValueShortWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueShortWithCP24Time2a
MeasuredValueShortWithCP24Time2a_getFromBuffer(MeasuredValueShortWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueShortWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueShortWithCP24Time2a));

        if (self != NULL)
            MeasuredValueShortWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueFloatWithCP56Time2a
 *******************************************/

struct sMeasuredValueShortWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
MeasuredValueShortWithCP56Time2a_encode(MeasuredValueShortWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueShort_encode((MeasuredValueShort) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT measuredValueShortWithCP56Time2aVFT = {
        (EncodeFunction) MeasuredValueShortWithCP56Time2a_encode,
        (DestroyFunction) MeasuredValueShortWithCP56Time2a_destroy
};

static void
MeasuredValueShortWithCP56Time2a_initialize(MeasuredValueShortWithCP56Time2a self)
{
    self->virtualFunctionTable = &(measuredValueShortWithCP56Time2aVFT);
    self->type = M_ME_TF_1;
}

void
MeasuredValueShortWithCP56Time2a_destroy(MeasuredValueShortWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueShortWithCP56Time2a
MeasuredValueShortWithCP56Time2a_create(MeasuredValueShortWithCP56Time2a self, int ioa,
        float value, QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueShortWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueShortWithCP56Time2a));

    if (self != NULL)
        MeasuredValueShortWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
MeasuredValueShortWithCP56Time2a_getTimestamp(MeasuredValueShortWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueShortWithCP56Time2a_setTimestamp(MeasuredValueShortWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueShortWithCP56Time2a
MeasuredValueShortWithCP56Time2a_getFromBuffer(MeasuredValueShortWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueShortWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueShortWithCP56Time2a));

        if (self != NULL)
            MeasuredValueShortWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * IntegratedTotals
 *******************************************/

struct sIntegratedTotals {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;
};

static void
IntegratedTotals_encode(IntegratedTotals self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_appendBytes(frame, self->totals.encodedValue, 5);
}

struct sInformationObjectVFT integratedTotalsVFT = {
        (EncodeFunction) IntegratedTotals_encode,
        (DestroyFunction) IntegratedTotals_destroy
};

static void
IntegratedTotals_initialize(IntegratedTotals self)
{
    self->virtualFunctionTable = &(integratedTotalsVFT);
    self->type = M_IT_NA_1;
}

void
IntegratedTotals_destroy(IntegratedTotals self)
{
    GLOBAL_FREEMEM(self);
}

IntegratedTotals
IntegratedTotals_create(IntegratedTotals self, int ioa, BinaryCounterReading value)
{
    if (self == NULL)
        self = (IntegratedTotals) GLOBAL_CALLOC(1, sizeof(struct sIntegratedTotals));

    if (self != NULL)
        IntegratedTotals_initialize(self);

    self->objectAddress = ioa;
    self->totals = *value;

    return self;
}

BinaryCounterReading
IntegratedTotals_getBCR(IntegratedTotals self)
{
    return &(self->totals);
}

void
IntegratedTotals_setBCR(IntegratedTotals self, BinaryCounterReading value)
{
    int i;

    for (i = 0; i < 5; i++)
        self->totals.encodedValue[i] = value->encodedValue[i];
}

IntegratedTotals
IntegratedTotals_getFromBuffer(IntegratedTotals self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (IntegratedTotals) GLOBAL_MALLOC(sizeof(struct sIntegratedTotals));

        if (self != NULL)
            IntegratedTotals_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* BCR */
        int i = 0;

        for (i = 0; i < 5; i++)
            self->totals.encodedValue[i] = msg [startIndex++];
    }

    return self;
}

/***********************************************************************
 * IntegratedTotalsWithCP24Time2a : IntegratedTotals
 ***********************************************************************/

struct sIntegratedTotalsWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;

    struct sCP24Time2a timestamp;
};

static void
IntegratedTotalsWithCP24Time2a_encode(IntegratedTotalsWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    IntegratedTotals_encode((IntegratedTotals) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT integratedTotalsWithCP24Time2aVFT = {
        (EncodeFunction) IntegratedTotalsWithCP24Time2a_encode,
        (DestroyFunction) IntegratedTotalsWithCP24Time2a_destroy
};

static void
IntegratedTotalsWithCP24Time2a_initialize(IntegratedTotalsWithCP24Time2a self)
{
    self->virtualFunctionTable = &(integratedTotalsWithCP24Time2aVFT);
    self->type = M_IT_TA_1;
}

void
IntegratedTotalsWithCP24Time2a_destroy(IntegratedTotalsWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

IntegratedTotalsWithCP24Time2a
IntegratedTotalsWithCP24Time2a_create(IntegratedTotalsWithCP24Time2a self, int ioa,
        BinaryCounterReading value, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (IntegratedTotalsWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sIntegratedTotalsWithCP24Time2a));

    if (self != NULL)
        IntegratedTotalsWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->totals = *value;
    self->timestamp = *timestamp;

    return self;
}


CP24Time2a
IntegratedTotalsWithCP24Time2a_getTimestamp(IntegratedTotalsWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
IntegratedTotalsWithCP24Time2a_setTimestamp(IntegratedTotalsWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}


IntegratedTotalsWithCP24Time2a
IntegratedTotalsWithCP24Time2a_getFromBuffer(IntegratedTotalsWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (IntegratedTotalsWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sIntegratedTotalsWithCP24Time2a));

        if (self != NULL)
            IntegratedTotalsWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* BCR */
        int i = 0;

        for (i = 0; i < 5; i++)
            self->totals.encodedValue[i] = msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***********************************************************************
 * IntegratedTotalsWithCP56Time2a : IntegratedTotals
 ***********************************************************************/

struct sIntegratedTotalsWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;

    struct sCP56Time2a timestamp;
};

static void
IntegratedTotalsWithCP56Time2a_encode(IntegratedTotalsWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    IntegratedTotals_encode((IntegratedTotals) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT integratedTotalsWithCP56Time2aVFT = {
        (EncodeFunction) IntegratedTotalsWithCP56Time2a_encode,
        (DestroyFunction) IntegratedTotalsWithCP56Time2a_destroy
};

static void
IntegratedTotalsWithCP56Time2a_initialize(IntegratedTotalsWithCP56Time2a self)
{
    self->virtualFunctionTable = &(integratedTotalsWithCP56Time2aVFT);
    self->type = M_IT_TB_1;
}

void
IntegratedTotalsWithCP56Time2a_destroy(IntegratedTotalsWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

IntegratedTotalsWithCP56Time2a
IntegratedTotalsWithCP56Time2a_create(IntegratedTotalsWithCP56Time2a self, int ioa,
        BinaryCounterReading value, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (IntegratedTotalsWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sIntegratedTotalsWithCP56Time2a));

    if (self != NULL)
        IntegratedTotalsWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->totals = *value;
    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
IntegratedTotalsWithCP56Time2a_getTimestamp(IntegratedTotalsWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
IntegratedTotalsWithCP56Time2a_setTimestamp(IntegratedTotalsWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}


IntegratedTotalsWithCP56Time2a
IntegratedTotalsWithCP56Time2a_getFromBuffer(IntegratedTotalsWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
		self = (IntegratedTotalsWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sIntegratedTotalsWithCP56Time2a));

        if (self != NULL)
            IntegratedTotalsWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* BCR */
        int i = 0;

        for (i = 0; i < 5; i++)
            self->totals.encodedValue[i] = msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * SingleCommand
 *******************************************/

struct sSingleCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t sco;
};

static void
SingleCommand_encode(SingleCommand self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->sco);
}

struct sInformationObjectVFT singleCommandVFT = {
        (EncodeFunction) SingleCommand_encode,
        (DestroyFunction) SingleCommand_destroy
};

static void
SingleCommand_initialize(SingleCommand self)
{
    self->virtualFunctionTable = &(singleCommandVFT);
    self->type = C_SC_NA_1;
}

void
SingleCommand_destroy(SingleCommand self)
{
    GLOBAL_FREEMEM(self);
}

SingleCommand
SingleCommand_create(SingleCommand self, int ioa, bool command, bool selectCommand, int qu)
{
    if (self == NULL) {
		self = (SingleCommand) GLOBAL_MALLOC(sizeof(struct sSingleCommand));

        if (self == NULL)
            return NULL;
        else
            SingleCommand_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t sco = ((qu & 0x1f) * 4);

    if (command) sco |= 0x01;

    if (selectCommand) sco |= 0x80;

    self->sco = sco;

    return self;
}

int
SingleCommand_getQU(SingleCommand self)
{
    return ((self->sco & 0x7c) / 4);
}

bool
SingleCommand_getState(SingleCommand self)
{
    return ((self->sco & 0x01) == 0x01);
}

bool
SingleCommand_isSelect(SingleCommand self)
{
    return ((self->sco & 0x80) == 0x80);
}

SingleCommand
SingleCommand_getFromBuffer(SingleCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (SingleCommand) GLOBAL_MALLOC(sizeof(struct sSingleCommand));

        if (self != NULL)
            SingleCommand_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->sco = msg[startIndex];
    }

    return self;
}


/***********************************************************************
 * SingleCommandWithCP56Time2a : SingleCommand
 ***********************************************************************/

struct sSingleCommandWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t sco;

    struct sCP56Time2a timestamp;
};

static void
SingleCommandWithCP56Time2a_encode(SingleCommandWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    SingleCommand_encode((SingleCommand) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT singleCommandWithCP56Time2aVFT = {
        (EncodeFunction) SingleCommandWithCP56Time2a_encode,
        (DestroyFunction) SingleCommandWithCP56Time2a_destroy
};

static void
SingleCommandWithCP56Time2a_initialize(SingleCommandWithCP56Time2a self)
{
    self->virtualFunctionTable = &(singleCommandWithCP56Time2aVFT);
    self->type = C_SC_TA_1;
}

void
SingleCommandWithCP56Time2a_destroy(SingleCommandWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_create(SingleCommandWithCP56Time2a self, int ioa, bool command, bool selectCommand, int qu, CP56Time2a timestamp)
{
    if (self == NULL) {
		self = (SingleCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSingleCommandWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            SingleCommandWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t sco = ((qu & 0x1f) * 4);

    if (command) sco |= 0x01;

    if (selectCommand) sco |= 0x80;

    self->sco = sco;

    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = timestamp->encodedValue[i];
    }

    return self;
}

CP56Time2a
SingleCommandWithCP56Time2a_getTimestamp(SingleCommandWithCP56Time2a self)
{
    return &(self->timestamp);
}

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_getFromBuffer(SingleCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (SingleCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSingleCommandWithCP56Time2a));

        if (self != NULL)
            SingleCommandWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->sco = msg[startIndex];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * DoubleCommand : InformationObject
 *******************************************/

struct sDoubleCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;
};

static void
DoubleCommand_encode(DoubleCommand self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->dcq);
}

struct sInformationObjectVFT doubleCommandVFT = {
        (EncodeFunction) DoubleCommand_encode,
        (DestroyFunction) DoubleCommand_destroy
};

static void
DoubleCommand_initialize(DoubleCommand self)
{
    self->virtualFunctionTable = &(doubleCommandVFT);
    self->type = C_DC_NA_1;
}

void
DoubleCommand_destroy(DoubleCommand self)
{
    GLOBAL_FREEMEM(self);
}

DoubleCommand
DoubleCommand_create(DoubleCommand self, int ioa, int command, bool selectCommand, int qu)
{
    if (self == NULL) {
		self = (DoubleCommand) GLOBAL_MALLOC(sizeof(struct sDoubleCommand));

        if (self == NULL)
            return NULL;
        else
            DoubleCommand_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t dcq = ((qu & 0x1f) * 4);

    dcq += (uint8_t) (command & 0x03);

    if (selectCommand) dcq |= 0x80;

    self->dcq = dcq;

    return self;
}

int
DoubleCommand_getQU(DoubleCommand self)
{
    return ((self->dcq & 0x7c) / 4);
}

int
DoubleCommand_getState(DoubleCommand self)
{
    return (self->dcq & 0x03);
}

bool
DoubleCommand_isSelect(DoubleCommand self)
{
    return ((self->dcq & 0x80) == 0x80);
}

DoubleCommand
DoubleCommand_getFromBuffer(DoubleCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (DoubleCommand) GLOBAL_MALLOC(sizeof(struct sDoubleCommand));

        if (self != NULL)
            DoubleCommand_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->dcq = msg[startIndex];
    }

    return self;
}

/*******************************************
 * StepCommand : InformationObject
 *******************************************/

struct sStepCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;
};

static void
StepCommand_encode(StepCommand self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->dcq);
}

struct sInformationObjectVFT stepCommandVFT = {
        (EncodeFunction) StepCommand_encode,
        (DestroyFunction) StepCommand_destroy
};

static void
StepCommand_initialize(StepCommand self)
{
    self->virtualFunctionTable = &(stepCommandVFT);
    self->type = C_RC_NA_1;
}

void
StepCommand_destroy(StepCommand self)
{
    GLOBAL_FREEMEM(self);
}

StepCommand
StepCommand_create(StepCommand self, int ioa, int command, bool selectCommand, int qu)
{
    if (self == NULL) {
		self = (StepCommand) GLOBAL_MALLOC(sizeof(struct sStepCommand));

        if (self == NULL)
            return NULL;
        else
            StepCommand_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t dcq = ((qu & 0x1f) * 4);

    dcq += (uint8_t) (command & 0x03);

    if (selectCommand) dcq |= 0x80;

    self->dcq = dcq;

    return self;
}

int
StepCommand_getQU(StepCommand self)
{
    return ((self->dcq & 0x7c) / 4);
}

int
StepCommand_getState(StepCommand self)
{
    return (self->dcq & 0x03);
}

bool
StepCommand_isSelect(StepCommand self)
{
    return ((self->dcq & 0x80) == 0x80);
}

StepCommand
StepCommand_getFromBuffer(StepCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (StepCommand) GLOBAL_MALLOC(sizeof(struct sStepCommand));

        if (self != NULL)
            StepCommand_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->dcq = msg[startIndex];
    }

    return self;
}

/*************************************************
 * SetpointCommandNormalized : InformationObject
 ************************************************/

struct sSetpointCommandNormalized {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */
};

static void
SetpointCommandNormalized_encode(SetpointCommandNormalized self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_appendBytes(frame, self->encodedValue, 2);
    Frame_setNextByte(frame, self->qos);
}

struct sInformationObjectVFT setpointCommandNormalizedVFT = {
        (EncodeFunction) SetpointCommandNormalized_encode,
        (DestroyFunction) SetpointCommandNormalized_destroy
};

static void
SetpointCommandNormalized_initialize(SetpointCommandNormalized self)
{
    self->virtualFunctionTable = &(setpointCommandNormalizedVFT);
    self->type = C_SE_NA_1;
}

void
SetpointCommandNormalized_destroy(SetpointCommandNormalized self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandNormalized
SetpointCommandNormalized_create(SetpointCommandNormalized self, int ioa, float value, bool selectCommand, int ql)
{
    if (self == NULL) {
		self = (SetpointCommandNormalized) GLOBAL_MALLOC(sizeof(struct sSetpointCommandNormalized));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandNormalized_initialize(self);
    }

    self->objectAddress = ioa;

    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, scaledValue);

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    return self;
}

float
SetpointCommandNormalized_getValue(SetpointCommandNormalized self)
{
    float nv = (float) (getScaledValue(self->encodedValue)) / 32767.f;

    return nv;
}

int
SetPointCommandNormalized_getQL(SetpointCommandNormalized self)
{
    return (int) (self->qos & 0x7f);
}

bool
SetpointCommandNormalized_isSelect(SetpointCommandNormalized self)
{
    return ((self->qos & 0x80) == 0x80);
}

SetpointCommandNormalized
SetpointCommandNormalized_getFromBuffer(SetpointCommandNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 3))
        return NULL;

    if (self == NULL) {
		self = (SetpointCommandNormalized) GLOBAL_MALLOC(sizeof(struct sSetpointCommandNormalized));

        if (self != NULL)
            SetpointCommandNormalized_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg[startIndex++];
        self->encodedValue[1] = msg[startIndex++];

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex];
    }

    return self;
}

/*************************************************
 * SetpointCommandScaled: InformationObject
 ************************************************/

struct sSetpointCommandScaled {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */
};

static void
SetpointCommandScaled_encode(SetpointCommandScaled self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_appendBytes(frame, self->encodedValue, 2);
    Frame_setNextByte(frame, self->qos);
}

struct sInformationObjectVFT setpointCommandScaledVFT = {
        (EncodeFunction) SetpointCommandScaled_encode,
        (DestroyFunction) SetpointCommandScaled_destroy
};

static void
SetpointCommandScaled_initialize(SetpointCommandScaled self)
{
    self->virtualFunctionTable = &(setpointCommandScaledVFT);
    self->type = C_SE_NB_1;
}

void
SetpointCommandScaled_destroy(SetpointCommandScaled self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandScaled
SetpointCommandScaled_create(SetpointCommandScaled self, int ioa, float value, bool selectCommand, int ql)
{
    if (self == NULL) {
		self = (SetpointCommandScaled) GLOBAL_MALLOC(sizeof(struct sSetpointCommandScaled));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandScaled_initialize(self);
    }

    self->objectAddress = ioa;

    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, scaledValue);

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    return self;
}

int
SetpointCommandScaled_getValue(SetpointCommandScaled self)
{
    return getScaledValue(self->encodedValue);
}

int
SetpointCommandScaled_getQL(SetpointCommandScaled self)
{
    return (int) (self->qos & 0x7f);
}

bool
SetpointCommandScaled_isSelect(SetpointCommandScaled self)
{
    return ((self->qos & 0x80) == 0x80);
}

SetpointCommandScaled
SetpointCommandScaled_getFromBuffer(SetpointCommandScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 3))
        return NULL;

    if (self == NULL) {
		self = (SetpointCommandScaled) GLOBAL_MALLOC(sizeof(struct sSetpointCommandScaled));

        if (self != NULL)
            SetpointCommandScaled_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg[startIndex++];
        self->encodedValue[1] = msg[startIndex++];

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex];
    }

    return self;
}

/*************************************************
 * SetpointCommandShort: InformationObject
 ************************************************/

struct sSetpointCommandShort {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    uint8_t qos; /* Qualifier of setpoint command */
};

static void
SetpointCommandShort_encode(SetpointCommandShort self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
    Frame_appendBytes(frame, valueBytes, 4);
#else
    Frame_setNextByte(frame, valueBytes[3]);
    Frame_setNextByte(frame, valueBytes[2]);
    Frame_setNextByte(frame, valueBytes[1]);
    Frame_setNextByte(frame, valueBytes[0]);
#endif

    Frame_setNextByte(frame, self->qos);
}

struct sInformationObjectVFT setpointCommandShortVFT = {
        (EncodeFunction) SetpointCommandShort_encode,
        (DestroyFunction) SetpointCommandShort_destroy
};

static void
SetpointCommandShort_initialize(SetpointCommandShort self)
{
    self->virtualFunctionTable = &(setpointCommandShortVFT);
    self->type = C_SE_NC_1;
}

void
SetpointCommandShort_destroy(SetpointCommandShort self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandShort
SetpointCommandShort_create(SetpointCommandShort self, int ioa, float value, bool selectCommand, int ql)
{
    if (self == NULL) {
		self = (SetpointCommandShort) GLOBAL_MALLOC(sizeof(struct sSetpointCommandShort));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandShort_initialize(self);
    }

    self->objectAddress = ioa;

    self->value = value;

    uint8_t qos = ql & 0x7f;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    return self;
}

float
SetpointCommandShort_getValue(SetpointCommandShort self)
{
    return self->value;
}

int
SetpointCommandShort_getQL(SetpointCommandShort self)
{
    return (int) (self->qos & 0x7f);
}

bool
SetpointCommandShort_isSelect(SetpointCommandShort self)
{
    return ((self->qos & 0x80) == 0x80);
}

SetpointCommandShort
SetpointCommandShort_getFromBuffer(SetpointCommandShort self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 5))
        return NULL;

    if (self == NULL) {
		self = (SetpointCommandShort) GLOBAL_MALLOC(sizeof(struct sSetpointCommandShort));

        if (self != NULL)
            SetpointCommandShort_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif


        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex];
    }

    return self;
}

/*************************************************
 * Bitstring32Command : InformationObject
 ************************************************/

struct sBitstring32Command {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
};

static void
Bitstring32Command_encode(Bitstring32Command self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
    Frame_appendBytes(frame, valueBytes, 4);
#else
    Frame_setNextByte(frame, valueBytes[3]);
    Frame_setNextByte(frame, valueBytes[2]);
    Frame_setNextByte(frame, valueBytes[1]);
    Frame_setNextByte(frame, valueBytes[0]);
#endif
}

struct sInformationObjectVFT bitstring32CommandVFT = {
        (EncodeFunction) Bitstring32Command_encode,
        (DestroyFunction) Bitstring32Command_destroy
};

static void
Bitstring32Command_initialize(Bitstring32Command self)
{
    self->virtualFunctionTable = &(bitstring32CommandVFT);
    self->type = C_BO_NA_1;
}

Bitstring32Command
Bitstring32Command_create(Bitstring32Command self, int ioa, uint32_t value)
{
    if (self == NULL) {
		self = (Bitstring32Command) GLOBAL_MALLOC(sizeof(struct sBitstring32Command));

        if (self == NULL)
            return NULL;
        else
            Bitstring32Command_initialize(self);
    }

    self->objectAddress = ioa;

    self->value = value;

    return self;
}

void
Bitstring32Command_destroy(Bitstring32Command self)
{
    GLOBAL_FREEMEM(self);
}

uint32_t
Bitstring32Command_getValue(Bitstring32Command self)
{
    return self->value;
}

Bitstring32Command
Bitstring32Command_getFromBuffer(Bitstring32Command self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 4))
        return NULL;

    if (self == NULL) {
		self = (Bitstring32Command) GLOBAL_MALLOC(sizeof(struct sBitstring32Command));

        if (self != NULL)
            Bitstring32Command_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif
    }

    return self;
}


/*************************************************
 * ReadCommand : InformationObject
 ************************************************/

struct sReadCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;
};

static void
ReadCommand_encode(ReadCommand self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);
}

struct sInformationObjectVFT readCommandVFT = {
        (EncodeFunction) ReadCommand_encode,
        (DestroyFunction) ReadCommand_destroy
};

static void
ReadCommand_initialize(ReadCommand self)
{
    self->virtualFunctionTable = &(readCommandVFT);
    self->type = C_RD_NA_1;
}

ReadCommand
ReadCommand_create(ReadCommand self, int ioa)
{
    if (self == NULL) {
		self = (ReadCommand) GLOBAL_MALLOC(sizeof(struct sReadCommand));

        if (self == NULL)
            return NULL;
        else
            ReadCommand_initialize(self);
    }

    self->objectAddress = ioa;

    return self;
}

void
ReadCommand_destroy(ReadCommand self)
{
    GLOBAL_FREEMEM(self);
}


ReadCommand
ReadCommand_getFromBuffer(ReadCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA))
        return NULL;

    if (self == NULL) {
		self = (ReadCommand) GLOBAL_MALLOC(sizeof(struct sReadCommand));

        if (self != NULL)
            ReadCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);
    }

    return self;
}

/***************************************************
 * ClockSynchronizationCommand : InformationObject
 **************************************************/

struct sClockSynchronizationCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sCP56Time2a timestamp;
};

static void
ClockSynchronizationCommand_encode(ClockSynchronizationCommand self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT clockSynchronizationCommandVFT = {
        (EncodeFunction) ClockSynchronizationCommand_encode,
        (DestroyFunction) ClockSynchronizationCommand_destroy
};

static void
ClockSynchronizationCommand_initialize(ClockSynchronizationCommand self)
{
    self->virtualFunctionTable = &(clockSynchronizationCommandVFT);
    self->type = C_CS_NA_1;
}

ClockSynchronizationCommand
ClockSynchronizationCommand_create(ClockSynchronizationCommand self, int ioa)
{
    if (self == NULL) {
		self = (ClockSynchronizationCommand) GLOBAL_MALLOC(sizeof(struct sClockSynchronizationCommand));

        if (self == NULL)
            return NULL;
        else
            ClockSynchronizationCommand_initialize(self);
    }

    self->objectAddress = ioa;

    return self;
}

void
ClockSynchronizationCommand_destroy(ClockSynchronizationCommand self)
{
    GLOBAL_FREEMEM(self);
}

CP56Time2a
ClockSynchronizationCommand_getTime(ClockSynchronizationCommand self)
{
    return &(self->timestamp);
}

ClockSynchronizationCommand
ClockSynchronizationCommand_getFromBuffer(ClockSynchronizationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 7)
        return NULL;

    if (self == NULL) {
		self = (ClockSynchronizationCommand) GLOBAL_MALLOC(sizeof(struct sClockSynchronizationCommand));

        if (self != NULL)
            ClockSynchronizationCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*************************************************
 * InterrogationCommand : InformationObject
 ************************************************/

struct sInterrogationCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t qoi;
};

static void
InterrogationCommand_encode(InterrogationCommand self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->qoi);
}

struct sInformationObjectVFT interrogationCommandVFT = {
        (EncodeFunction) InterrogationCommand_encode,
        (DestroyFunction) InterrogationCommand_destroy
};

static void
InterrogationCommand_initialize(InterrogationCommand self)
{
    self->virtualFunctionTable = &(interrogationCommandVFT);
    self->type = C_IC_NA_1;
}

InterrogationCommand
InterrogationCommand_create(InterrogationCommand self, int ioa, uint8_t qoi)
{
    if (self == NULL) {
		self = (InterrogationCommand) GLOBAL_MALLOC(sizeof(struct sInterrogationCommand));

        if (self == NULL)
            return NULL;
        else
            InterrogationCommand_initialize(self);
    }

    self->objectAddress = ioa;

    self->qoi = qoi;

    return self;
}

void
InterrogationCommand_destroy(InterrogationCommand self)
{
    GLOBAL_FREEMEM(self);
}

uint8_t
InterrogationCommand_getQOI(InterrogationCommand self)
{
    return self->qoi;
}

InterrogationCommand
InterrogationCommand_getFromBuffer(InterrogationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 1)
        return NULL;

    if (self == NULL) {
		self = (InterrogationCommand) GLOBAL_MALLOC(sizeof(struct sInterrogationCommand));

        if (self != NULL)
            InterrogationCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* QUI */
        self->qoi = msg[startIndex];
    }

    return self;
}


union uInformationObject {
    struct sSinglePointInformation m1;
    struct sStepPositionInformation m2;
    struct sStepPositionWithCP24Time2a m3;
    struct sStepPositionWithCP56Time2a m4;
    struct sDoublePointInformation m5;
    struct sDoublePointWithCP24Time2a m6;
    struct sDoublePointWithCP56Time2a m7;
    struct sSinglePointWithCP24Time2a m8;
    struct sSinglePointWithCP56Time2a m9;
    struct sBitString32 m10;
    struct sBitstring32WithCP24Time2a m11;
    struct sBitstring32WithCP56Time2a m12;
    struct sMeasuredValueNormalized m13;
    struct sMeasuredValueNormalizedWithCP24Time2a m14;
    struct sMeasuredValueNormalizedWithCP56Time2a m15;
    struct sMeasuredValueScaled m16;
    struct sMeasuredValueScaledWithCP24Time2a m17;
    struct sMeasuredValueScaledWithCP56Time2a m18;
    struct sMeasuredValueShort m19;
    struct sMeasuredValueShortWithCP24Time2a m20;
    struct sMeasuredValueShortWithCP56Time2a m21;
    struct sIntegratedTotals m22;
    struct sIntegratedTotalsWithCP24Time2a m23;
    struct sIntegratedTotalsWithCP56Time2a m24;
    struct sSingleCommand m25;
    struct sSingleCommandWithCP56Time2a m26;
    struct sDoubleCommand m27;
    struct sStepCommand m28;
    struct sSetpointCommandNormalized m29;
    struct sSetpointCommandScaled m30;
    struct sSetpointCommandShort m31;
    struct sBitstring32Command m32;
    struct sReadCommand m33;
    struct sClockSynchronizationCommand m34;
    struct sInterrogationCommand m35;
};

int
InformationObject_getMaxSizeInMemory()
{
    return sizeof(union uInformationObject);
}