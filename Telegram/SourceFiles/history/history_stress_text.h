#pragma once

#include "base/assertion.h"
#include "ui/text/text_entity.h"

#include <memory>

class HistoryItem;

namespace Iv {
struct RichPage;
} // namespace Iv

namespace HistoryStress {

struct DisplayResult {
	TextWithEntities text;
	std::shared_ptr<const Iv::RichPage> page;
};

[[nodiscard]] DisplayResult Display(
	not_null<HistoryItem*> item,
	const TextWithEntities &text,
	std::shared_ptr<const Iv::RichPage> page);
[[nodiscard]] bool CanToggle(not_null<const HistoryItem*> item);
[[nodiscard]] bool ShowingOriginal(not_null<const HistoryItem*> item);
void ToggleOriginal(not_null<HistoryItem*> item);
[[nodiscard]] int ToOriginal(not_null<HistoryItem*> item, int offset);
[[nodiscard]] int ToMarked(not_null<HistoryItem*> item, int offset);

} // namespace HistoryStress
