#pragma once

#include "base/assertion.h"
#include "ui/text/text_entity.h"

#include <vector>

namespace HistoryStress {

struct MarkedText {
	TextWithEntities text;
	std::vector<int> originalToMarked;
	std::vector<int> markedToOriginal;
	bool failed = false;
};

[[nodiscard]] bool HasRussian(const QString &text);
[[nodiscard]] MarkedText Mark(const TextWithEntities &text);

} // namespace HistoryStress
