#ifndef UNDO_HPP
#define UNDO_HPP

#include "src/DistrhoDefines.h"
#include "Defines.hpp"
START_NAMESPACE_DISTRHO
#define MAX_UNDO_DEPTH 12


struct UndoItem
{
    std::vector<float> data;
    std::vector<std::atomic<float>>*atomicDataPtr;
    bool isAtomic=false;
    bool shouldContinue;
    // UndoItem(std::vector<float> *_data, bool _shouldContinue=false)
    // {
    //     data.reserve(MAX_SAMPLE_LENGTH);
    //     data=*_data;
    //     dataPtr=_data;
    //     shouldContinue=_shouldContinue;
    // }
    UndoItem(std::vector<std::atomic<float>> *_data, bool _shouldContinue=false)
    {
        data.reserve(MAX_SAMPLE_LENGTH);
        for(int i=0;i<_data->size();i++)
            data.push_back((*_data)[i].load(std::memory_order_relaxed));
        atomicDataPtr=_data;
        isAtomic=true;
        shouldContinue=_shouldContinue;
    }
    void apply()
    {
        if(isAtomic)
        {
            for(int i=0;i<data.size();i++)
            {
                (*atomicDataPtr)[i].store(data[i],std::memory_order_relaxed);
            }

        }else{
            // *dataPtr=data;
        }

    }
};


END_NAMESPACE_DISTRHO

#endif // UNDO_HPP
