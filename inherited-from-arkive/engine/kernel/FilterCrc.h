#ifndef FILTERCRC_H
#define FILTERCRC_H

#include "kernel/Filter.h"

class FilterCrc : public Filter {
public:
    FilterCrc();
    virtual void filterData(QByteArray& data, QByteArray *filteredData);
    virtual void filterLast(QByteArray& data, QByteArray *filteredData);
    virtual int blockSize();

    quint32 crcValue();
    void reset();

private:
    quint32 mCrc;
};

#endif
