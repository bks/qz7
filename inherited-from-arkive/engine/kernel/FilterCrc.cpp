#include "kernel/FilterCrc.h"
#include "kernel/Crc.h"

FilterCrc::FilterCrc()
    : mCrc(CrcInitValue())
{
}

void FilterCrc::filterData(QByteArray& data, QByteArray *filteredData)
{
    mCrc = CrcUpdate(mCrc, data.constData(), data.size());
    *filteredData = data;
}

void FilterCrc::filterLast(QByteArray& data, QByteArray *filteredData)
{
    filterData(data, filteredData);
}

int FilterCrc::blockSize()
{
    return 1;
}

quint32 FilterCrc::crcValue()
{
    return CrcValue(mCrc);
}

void FilterCrc::reset()
{
    mCrc = CrcInitValue();
}
