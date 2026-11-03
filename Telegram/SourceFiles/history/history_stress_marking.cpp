#include "history/history_stress_marking.h"

#include "exception_dictionary_wrapper.h"
#include "predict.h"
#include "session.h"

#include <algorithm>
#include <exception>
#include <mutex>

namespace HistoryStress {
namespace {

[[nodiscard]] bool IsRussian(QChar ch) {
	const auto code = ch.unicode();
	return (code >= u'А' && code <= u'я') || code == u'Ё' || code == u'ё';
}

[[nodiscard]] bool ProtectEntity(EntityType type) {
	switch (type) {
	case EntityType::Url:
	case EntityType::Email:
	case EntityType::Phone:
	case EntityType::Mention:
	case EntityType::Hashtag:
	case EntityType::Cashtag:
	case EntityType::BotCommand:
	case EntityType::Code:
	case EntityType::Pre:
	case EntityType::CustomEmoji:
	case EntityType::FormattedDate:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] QString MaskProtected(const TextWithEntities &input) {
	auto result = input.text;
	const auto length = int(result.size());
	auto protectedEntities = input.entities;
	const auto detected = TextUtilities::ParseEntities(
		input.text,
		TextParseLinks | TextParseMentions | TextParseHashtags | TextParseBotCommands);
	protectedEntities.append(detected.entities);
	for (const auto &entity : protectedEntities) {
		if (ProtectEntity(entity.type())) {
			const auto from = std::clamp(entity.offset(), 0, length);
			const auto till = std::clamp(
				entity.offset() + entity.length(),
				from,
				length);
			for (auto i = from; i != till; ++i) {
				result[i] = QChar(u' ');
			}
		}
	}
	for (auto from = 0; from < length;) {
		if (!input.text[from].isLetter()) {
			++from;
			continue;
		}
		auto till = from;
		auto foreign = false;
		while (till < length
			&& (input.text[till].isLetter() || input.text[till].isMark())) {
			foreign = foreign
				|| (input.text[till].isLetter() && !IsRussian(input.text[till]));
			++till;
		}
		if (foreign) {
			for (auto i = from; i != till; ++i) {
				result[i] = QChar(u' ');
			}
		}
		from = till;
	}
	return result;
}

[[nodiscard]] MarkedText Reconstruct(
		const TextWithEntities &input,
		const QString &masked,
		const QString &prediction) {
	auto result = MarkedText{ .text = input };
	result.text.text.clear();
	result.originalToMarked.push_back(0);
	result.markedToOriginal.push_back(0);
	auto predicted = 0;
	for (auto i = 0; i != masked.size(); ++i) {
		if (predicted == prediction.size()
			|| prediction[predicted] != masked[i]) {
			return { .text = input, .failed = true };
		}
		++predicted;
		result.text.text += input.text[i];
		result.markedToOriginal.push_back(i + 1);
		if (predicted < prediction.size()
			&& prediction[predicted] == QChar(0x0301)
			&& (i + 1 == masked.size() || masked[i + 1] != QChar(0x0301))) {
			if (!IsRussian(masked[i])) {
				return { .text = input, .failed = true };
			}
			result.text.text += QChar(0x0301);
			result.markedToOriginal.push_back(i + 1);
			++predicted;
		}
		result.originalToMarked.push_back(int(result.text.text.size()));
	}
	if (predicted != prediction.size()) {
		return { .text = input, .failed = true };
	}
	for (auto &entity : result.text.entities) {
		const auto from = entity.offset();
		const auto till = from + entity.length();
		if (from < 0 || till < from || till > input.text.size()) {
			return { .text = input, .failed = true };
		}
		entity = EntityInText(
			entity.type(),
			result.originalToMarked[from],
			result.originalToMarked[till] - result.originalToMarked[from],
			entity.data());
	}
	return result;
}

} // namespace

bool HasRussian(const QString &text) {
	return std::any_of(text.begin(), text.end(), IsRussian);
}

MarkedText Mark(const TextWithEntities &text) {
	for (const auto &entity : text.entities) {
		if (entity.offset() < 0 || entity.length() < 0
			|| entity.offset() > text.text.size()
			|| entity.length() > text.text.size() - entity.offset()) {
			return { .text = text, .failed = true };
		}
	}
	if (!HasRussian(text.text)) {
		return { .text = text };
	}
	const auto masked = MaskProtected(text);
	if (!HasRussian(masked)) {
		return { .text = text };
	}
	try {
		static auto once = std::once_flag();
		std::call_once(once, [] {
			init_dict();
			init_session();
		});
		return Reconstruct(
			text,
			masked,
			QString::fromStdWString(mark_stresses(masked.toStdWString())));
	} catch (const std::exception &) {
		return { .text = text, .failed = true };
	}
}

} // namespace HistoryStress
