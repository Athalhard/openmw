#ifndef CSV_WORLD_CREATOR_H
#define CSV_WORLD_CREATOR_H

#include <QWidget>

class QUndoStack;

namespace CSMWorld
{
    class Data;
    class UniversalId;
}

namespace CSVWorld
{
    /// \brief Record creator UI base class
    class Creator : public QWidget
    {
            Q_OBJECT

        public:

            virtual ~Creator();

            virtual void reset() = 0;

            virtual void setEditLock (bool locked) = 0;

        signals:

            void done();

            void requestFocus (const std::string& id);
            ///< Request owner of this creator to focus the just created \a id. The owner may
            /// ignore this request.
    };

    /// \brief Base class for Creator factory
    class CreatorFactoryBase
    {
        public:

            virtual ~CreatorFactoryBase();

            virtual Creator *makeCreator (CSMWorld::Data& data, QUndoStack& undoStack,
                const CSMWorld::UniversalId& id) const = 0;
            ///< The ownership of the returned Creator is transferred to the caller.
            ///
            /// \note The function can return a 0-pointer, which means no UI for creating/deleting
            /// records should be provided.
    };

    /// \brief Creator factory that does not produces any creator
    class NullCreatorFactory : public CreatorFactoryBase
    {
        public:

            virtual Creator *makeCreator (CSMWorld::Data& data, QUndoStack& undoStack,
                const CSMWorld::UniversalId& id) const;
            ///< The ownership of the returned Creator is transferred to the caller.
            ///
            /// \note The function always returns 0.
    };

    template<class CreatorT>
    class CreatorFactory : public CreatorFactoryBase
    {
        public:

            virtual Creator *makeCreator (CSMWorld::Data& data, QUndoStack& undoStack,
                const CSMWorld::UniversalId& id) const;
            ///< The ownership of the returned Creator is transferred to the caller.
            ///
            /// \note The function can return a 0-pointer, which means no UI for creating/deleting
            /// records should be provided.
    };

    template<class CreatorT>
    Creator *CreatorFactory<CreatorT>::makeCreator (CSMWorld::Data& data, QUndoStack& undoStack,
        const CSMWorld::UniversalId& id) const
    {
        return new CreatorT (data, undoStack, id);
    }
}

#endif