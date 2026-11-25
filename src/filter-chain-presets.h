#pragma once
#include <obs.hpp>
#include <QByteArray>
#include <QList>
namespace augment {
// A snapshot of live filter references in native enumeration order.
using FilterChain=QList<OBSSource>;
FilterChain captureFilters(obs_source_t *source);
QByteArray encodeFilters(const FilterChain &filters);
bool prepareFilters(obs_source_t *parent,const QByteArray &bytes,FilterChain &prepared);
bool replaceFilters(obs_source_t *parent,const FilterChain &expected,const FilterChain &replacement);
}
