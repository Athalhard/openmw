#ifndef OPENMW_ESM_OBJECTSTATE_H
#define OPENMW_ESM_OBJECTSTATE_H

#include <string>
#include <vector>

#include "cellref.hpp"
#include "locals.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    // format 0, saved games only

    ///< \brief Save state for objects, that do not use custom data
    struct ObjectState
    {
        std::string mId;

        CellRef mRef;

        unsigned char mHasLocals;
        Locals mLocals;
        unsigned char mEnabled;
        int mCount;
        ESM::Position mPosition;
        float mLocalRotation[3];

        void load (ESMReader &esm);
        void save (ESMWriter &esm) const;
    };
}

#endif