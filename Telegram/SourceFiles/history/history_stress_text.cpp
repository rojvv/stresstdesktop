#include "history/history_stress_text.h"

#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_stress_marking.h"
#include "iv/iv_rich_page.h"

#include <crl/crl_on_main.h>
#include <crl/crl_queue.h>
#include <QtCore/QCoreApplication>

#include <atomic>

namespace HistoryStress {
namespace {

struct State {
	HistoryItem *item = nullptr;
	TextWithEntities input;
	MarkedText marked;
	std::shared_ptr<const Iv::RichPage> page;
	std::shared_ptr<const Iv::RichPage> markedPage;
	uint64 generation = 0;
	bool ready = false;
	bool original = false;
};

struct Component : RuntimeComponent<Component, HistoryItem> {
	std::shared_ptr<State> state;
};

class Worker {
public:
	Worker();
	std::atomic<bool> stopping = false;
	crl::queue queue;

};

Worker::Worker() {
	const auto app = QCoreApplication::instance();
	QObject::connect(app, &QCoreApplication::aboutToQuit, app, [this] {
		stopping.store(true, std::memory_order_relaxed);
		queue.sync([] {});
	});
}

[[nodiscard]] State *Lookup(not_null<const HistoryItem*> item) {
	const auto component = item->Get<Component>();
	return component ? component->state.get() : nullptr;
}

bool MarkRichBlocks(std::vector<Iv::RichPage::Block> &blocks) {
	auto changed = false;
	const auto mark = [&](TextWithEntities &text) {
		auto result = Mark(text);
		if (result.text != text) {
			text = std::move(result.text);
			changed = true;
		}
	};
	for (auto &block : blocks) {
		if (block.kind != Iv::RichPage::BlockKind::Code) {
			mark(block.text.text);
		}
		mark(block.caption.text);
		changed = MarkRichBlocks(block.blocks) || changed;
		for (auto &item : block.listItems) {
			mark(item.text.text);
			changed = MarkRichBlocks(item.blocks) || changed;
		}
		for (auto &row : block.tableRows) {
			for (auto &cell : row.cells) {
				mark(cell.text.text);
			}
		}
		for (auto &button : block.buttons) {
			mark(button.text.text);
		}
	}
	return changed;
}

[[nodiscard]] std::shared_ptr<const Iv::RichPage> MarkPage(
		const std::shared_ptr<const Iv::RichPage> &original) {
	if (!original) {
		return nullptr;
	}
	auto result = std::make_shared<Iv::RichPage>(*original);
	return MarkRichBlocks(result->blocks) ? result : original;
}

void Complete(
		const std::weak_ptr<State> &weak,
		uint64 generation,
		MarkedText marked,
		std::shared_ptr<const Iv::RichPage> page) {
	const auto state = weak.lock();
	if (!state || state->generation != generation) {
		return;
	}
	state->marked = std::move(marked);
	state->markedPage = std::move(page);
	state->ready = true;
	if (state->marked.failed) {
		LOG(("Stress marking failed; preserving the original message."));
	}
	state->item->history()->owner().requestItemTextRefresh(state->item);
}

void Request(const std::shared_ptr<State> &state) {
	static auto worker = Worker();
	const auto weak = std::weak_ptr<State>(state);
	const auto generation = ++state->generation;
	worker.queue.async([weak, generation, input = state->input,
			page = state->page, stopping = &worker.stopping] {
		if (stopping->load(std::memory_order_relaxed) || weak.expired()) {
			return;
		}
		auto marked = Mark(input);
		auto markedPage = MarkPage(page);
		crl::on_main([weak, generation,
			marked = std::move(marked),
			page = std::move(markedPage)]() mutable {
			Complete(weak, generation, std::move(marked), std::move(page));
		});
	});
}

[[nodiscard]] int MapOffset(
		not_null<HistoryItem*> item,
		int offset,
		bool toOriginal) {
	const auto state = Lookup(item);
	if (!state || !state->ready || state->original
		|| state->input.text != item->originalText().text) {
		return offset;
	}
	const auto &map = toOriginal
		? state->marked.markedToOriginal
		: state->marked.originalToMarked;
	return (offset >= 0 && offset < map.size()) ? map[offset] : offset;
}

} // namespace

DisplayResult Display(
		not_null<HistoryItem*> item,
		const TextWithEntities &text,
		std::shared_ptr<const Iv::RichPage> page) {
	if (ShowingOriginal(item)) {
		return { text, item->richPage() };
	}
	if (!HasRussian(text.text) && !page) {
		item->RemoveComponents(Component::Bit());
		return { text, nullptr };
	}
	item->AddComponents(Component::Bit());
	auto &state = item->Get<Component>()->state;
	if (!state) {
		state = std::make_shared<State>();
		state->item = item;
	}
	if (state->input != text || state->page != page) {
		state->input = text;
		state->page = page;
		state->ready = false;
		state->marked = {};
		state->markedPage = nullptr;
		Request(state);
	}
	return state->ready
		? DisplayResult{ state->marked.text, state->markedPage }
		: DisplayResult{ text, std::move(page) };
}

bool CanToggle(not_null<const HistoryItem*> item) {
	const auto state = Lookup(item);
	return state && state->ready
		&& (state->marked.text.text != state->input.text
			|| state->markedPage != state->page);
}

bool ShowingOriginal(not_null<const HistoryItem*> item) {
	const auto state = Lookup(item);
	return state && state->original;
}

void ToggleOriginal(not_null<HistoryItem*> item) {
	if (const auto state = Lookup(item)) {
		state->original = !state->original;
		item->history()->owner().requestItemTextRefresh(item);
	}
}

int ToOriginal(not_null<HistoryItem*> item, int offset) {
	return MapOffset(item, offset, true);
}

int ToMarked(not_null<HistoryItem*> item, int offset) {
	return MapOffset(item, offset, false);
}

} // namespace HistoryStress
