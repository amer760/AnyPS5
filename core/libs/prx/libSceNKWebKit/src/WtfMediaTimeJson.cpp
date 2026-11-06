#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/JsonExports.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::ObjectSetValue;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::Value;
using NKWebKit::WtfAbi::String;
using WTF::MediaTime;

const MediaTime& Checked(const MediaTime* self) {
    if (!self) throw std::invalid_argument("null WTF::MediaTime");
    return *self;
}

String Latin1(const std::string& text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

std::string FormatNumber(double value) {
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value < 0 ? "-Infinity" : "Infinity";
    return NKWebKit::WtfAbi::NumberToString(value);
}

void Set(Object* object, const char* key, Ref<Value>&& value) {
    ObjectSetValue(object, Latin1(key), std::move(value));
}

bool Has(const MediaTime& time, int flag) {
    return (time.m_timeFlags & flag) != 0;
}

}

String APS5_VABI _ZNK3WTF9MediaTime8toStringEv(const MediaTime* self) {
    const auto& time = Checked(self);
    const char* invalid = Has(time, MediaTime::Valid) ? "" : ", invalid";
    const std::string value = FormatNumber(_ZNK3WTF9MediaTime8toDoubleEv(&time));
    if (Has(time, MediaTime::DoubleValue)) return Latin1("{" + value + invalid + "}");
    return Latin1("{" + std::to_string(time.m_timeValue) + "/" + std::to_string(time.m_timeScale) + " = " + value + invalid + "}");
}

Ref<Object> APS5_VABI _ZNK3WTF9MediaTime12toJSONObjectEv(const MediaTime* self) {
    const auto& time = Checked(self);
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    if (Has(time, MediaTime::DoubleValue)) {
        Set(object.Get(), "value", _ZN3WTF8JSONImpl5Value6createEd(_ZNK3WTF9MediaTime8toDoubleEv(&time)));
        return object;
    }
    if (!Has(time, MediaTime::Valid))
        Set(object.Get(), "invalid", _ZN3WTF8JSONImpl5Value6createEb(true));
    else if (Has(time, MediaTime::Indefinite))
        Set(object.Get(), "value", _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("NaN")));
    else if (Has(time, MediaTime::PositiveInfinite))
        Set(object.Get(), "value", _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("POSITIVE_INFINITY")));
    else if (Has(time, MediaTime::NegativeInfinite))
        Set(object.Get(), "value", _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Latin1("NEGATIVE_INFINITY")));
    else
        Set(object.Get(), "value", _ZN3WTF8JSONImpl5Value6createEd(_ZNK3WTF9MediaTime8toDoubleEv(&time)));
    Set(object.Get(), "numerator", _ZN3WTF8JSONImpl5Value6createEd(static_cast<double>(time.m_timeValue)));
    Set(object.Get(), "denominator", _ZN3WTF8JSONImpl5Value6createEi(static_cast<int>(time.m_timeScale)));
    Set(object.Get(), "flags", _ZN3WTF8JSONImpl5Value6createEi(time.m_timeFlags));
    return object;
}

String APS5_VABI _ZNK3WTF9MediaTime12toJSONStringEv(const MediaTime* self) {
    auto object = _ZNK3WTF9MediaTime12toJSONObjectEv(self);
    return _ZNK3WTF8JSONImpl5Value12toJSONStringEv(object.Get());
}

String APS5_VABI _ZNK3WTF14MediaTimeRange12toJSONStringEv(const WTF::MediaTimeRange* self) {
    if (!self) throw std::invalid_argument("null WTF::MediaTimeRange");
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    Set(object.Get(), "start", _ZNK3WTF9MediaTime12toJSONObjectEv(&self->start));
    Set(object.Get(), "end", _ZNK3WTF9MediaTime12toJSONObjectEv(&self->end));
    return _ZNK3WTF8JSONImpl5Value12toJSONStringEv(object.Get());
}
