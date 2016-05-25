//-----------------------------------------------------------------------------
// Implementation of our Request class; a request is a user-created thing
// that will generate an entity (line, curve) when the sketch is generated,
// in the same way that other entities are generated automatically, like
// by an extrude or a step and repeat.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

const EntReqTable::TableEntry EntReqTable::Table[] = {
// request type                   entity type                 pts   xtra?   norml   dist    description
{ Request::Type::WORKPLANE,       Entity::Type::WORKPLANE,      1,  false,  true,   false, "workplane"      },
{ Request::Type::DATUM_POINT,     (Entity::Type)0,              1,  false,  false,  false, "datum-point"    },
{ Request::Type::LINE_SEGMENT,    Entity::Type::LINE_SEGMENT,   2,  false,  false,  false, "line-segment"   },
{ Request::Type::CUBIC,           Entity::Type::CUBIC,          4,  true,   false,  false, "cubic-bezier"   },
{ Request::Type::CUBIC_PERIODIC,  Entity::Type::CUBIC_PERIODIC, 3,  true,   false,  false, "periodic-cubic" },
{ Request::Type::CIRCLE,          Entity::Type::CIRCLE,         1,  false,  true,   true,  "circle"         },
{ Request::Type::ARC_OF_CIRCLE,   Entity::Type::ARC_OF_CIRCLE,  3,  false,  true,   false, "arc-of-circle"  },
{ Request::Type::TTF_TEXT,        Entity::Type::TTF_TEXT,       2,  false,  true,   false, "ttf-text"       },
{ (Request::Type)0,               (Entity::Type)0,              0,  false,  false,  false, NULL             },
};

const char *EntReqTable::DescriptionForRequest(Request::Type req) {
    for(int i = 0; Table[i].reqType != (Request::Type)0; i++) {
        if(req == Table[i].reqType) {
            return Table[i].description;
        }
    }
    return "???";
}

void EntReqTable::CopyEntityInfo(const TableEntry *te, int extraPoints,
           Entity::Type *ent, Request::Type *req, int *pts, bool *hasNormal, bool *hasDistance)
{
    int points = te->points;
    if(te->useExtraPoints) points += extraPoints;

    if(ent)         *ent         = te->entType;
    if(req)         *req         = te->reqType;
    if(pts)         *pts         = points;
    if(hasNormal)   *hasNormal   = te->hasNormal;
    if(hasDistance) *hasDistance = te->hasDistance;
}

bool EntReqTable::GetRequestInfo(Request::Type req, int extraPoints,
                     Entity::Type *ent, int *pts, bool *hasNormal, bool *hasDistance)
{
    for(int i = 0; Table[i].reqType != (Request::Type)0; i++) {
        const TableEntry *te = &(Table[i]);
        if(req == te->reqType) {
            CopyEntityInfo(te, extraPoints,
                ent, NULL, pts, hasNormal, hasDistance);
            return true;
        }
    }
    return false;
}

bool EntReqTable::GetEntityInfo(Entity::Type ent, int extraPoints,
                     Request::Type *req, int *pts, bool *hasNormal, bool *hasDistance)
{
    for(int i = 0; Table[i].reqType != (Request::Type)0; i++) {
        const TableEntry *te = &(Table[i]);
        if(ent == te->entType) {
            CopyEntityInfo(te, extraPoints,
                NULL, req, pts, hasNormal, hasDistance);
            return true;
        }
    }
    return false;
}

Request::Type EntReqTable::GetRequestForEntity(Entity::Type ent) {
    Request::Type req;
    ssassert(GetEntityInfo(ent, 0, &req, NULL, NULL, NULL),
             "No entity for request");
    return req;
}


void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Param,hParam> *param) const
{
    int points = 0;
    Entity::Type et;
    bool hasNormal = false;
    bool hasDistance = false;
    int i;

    Entity e = {};
    EntReqTable::GetRequestInfo(type, extraPoints, &et, &points, &hasNormal, &hasDistance);

    // Generate the entity that's specific to this request.
    e.type = et;
    e.extraPoints = extraPoints;
    e.group = group;
    e.style = style;
    e.workplane = workplane;
    e.construction = construction;
    e.str = str;
    e.font = font;
    e.h = h.entity(0);

    // And generate entities for the points
    for(i = 0; i < points; i++) {
        Entity p = {};
        p.workplane = workplane;
        // points start from entity 1, except for datum point case
        p.h = h.entity(i+((et != (Entity::Type)0) ? 1 : 0));
        p.group = group;
        p.style = style;

        if(workplane.v == Entity::FREE_IN_3D.v) {
            p.type = Entity::Type::POINT_IN_3D;
            // params for x y z
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
            p.param[2] = AddParam(param, h.param(16 + 3*i + 2));
        } else {
            p.type = Entity::Type::POINT_IN_2D;
            // params for u v
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
        }
        entity->Add(&p);
        e.point[i] = p.h;
    }
    if(hasNormal) {
        Entity n = {};
        n.workplane = workplane;
        n.h = h.entity(32);
        n.group = group;
        n.style = style;
        if(workplane.v == Entity::FREE_IN_3D.v) {
            n.type = Entity::Type::NORMAL_IN_3D;
            n.param[0] = AddParam(param, h.param(32+0));
            n.param[1] = AddParam(param, h.param(32+1));
            n.param[2] = AddParam(param, h.param(32+2));
            n.param[3] = AddParam(param, h.param(32+3));
        } else {
            n.type = Entity::Type::NORMAL_IN_2D;
            // and this is just a copy of the workplane quaternion,
            // so no params required
        }
        ssassert(points >= 1, "Positioning a normal requires a point");
        // The point determines where the normal gets displayed on-screen;
        // it's entirely cosmetic.
        n.point[0] = e.point[0];
        entity->Add(&n);
        e.normal = n.h;
    }
    if(hasDistance) {
        Entity d = {};
        d.workplane = workplane;
        d.h = h.entity(64);
        d.group = group;
        d.style = style;
        d.type = Entity::Type::DISTANCE;
        d.param[0] = AddParam(param, h.param(64));
        entity->Add(&d);
        e.distance = d.h;
    }

    if(et != (Entity::Type)0) entity->Add(&e);
}

std::string Request::DescriptionString() const {
    const char *s;
    if(h.v == Request::HREQUEST_REFERENCE_XY.v) {
        s = "#XY";
    } else if(h.v == Request::HREQUEST_REFERENCE_YZ.v) {
        s = "#YZ";
    } else if(h.v == Request::HREQUEST_REFERENCE_ZX.v) {
        s = "#ZX";
    } else {
        s = EntReqTable::DescriptionForRequest(type);
    }

    return ssprintf("r%03x-%s", h.v, s);
}

int Request::IndexOfPoint(hEntity he) const {
    if(type == Type::DATUM_POINT) {
        return (he.v == h.entity(0).v) ? 0 : -1;
    }
    for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
        if(he.v == h.entity(i + 1).v) {
            return i;
        }
    }
    return -1;
}

hParam Request::AddParam(IdList<Param,hParam> *param, hParam hp) {
    Param pa = {};
    pa.h = hp;
    param->Add(&pa);
    return hp;
}

