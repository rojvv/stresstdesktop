#include "history/history_stress_marking.h"

#include <crl/crl_on_main.h>
#include <rpl/never.h>

#include <iostream>
#include <stdexcept>

namespace crl {

rpl::producer<> on_main_update_requests() {
	// This formatting test has no window or animation update loop.
	return rpl::never<>();
}

} // namespace crl

namespace {

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

HistoryStress::MarkedText Check(
		const TextWithEntities &input,
		const QString &expected) {
	const auto result = HistoryStress::Mark(input);
	Require(!result.failed, "Inference failed");
	if (result.text.text != expected) {
		throw std::runtime_error("Unexpected prediction: "
			+ result.text.text.toStdString());
	}
	if (!result.originalToMarked.empty()) {
		Require(result.originalToMarked.size() == input.text.size() + 1,
			"Original offset map has the wrong length");
		Require(result.markedToOriginal.size() == result.text.text.size() + 1,
			"Display offset map has the wrong length");
		for (auto i = 0; i != result.originalToMarked.size(); ++i) {
			Require(result.markedToOriginal[result.originalToMarked[i]] == i,
				"Quote/edit offset did not round-trip");
		}
	}
	return result;
}

} // namespace

int main() {
	try {
		Check({ QString::fromUtf8("МОЛОКО, Привет! Это проверка ударений.") },
			QString::fromUtf8("МОЛОКО́, Приве́т! Э́то прове́рка ударе́ний."));
		Check({ QString::fromUtf8("🙂 молоко\tпредупреждали\n") },
			QString::fromUtf8("🙂 молоко́\tпредупрежда́ли\n"));
		Check({ QString::fromUtf8("молоко́ молоко́ ёж её") },
			QString::fromUtf8("молоко́ молоко́ ёж её"));
		Check({ QString::fromUtf8("привіт hello молоко") },
			QString::fromUtf8("привіт hello молоко́"));
		Check({ QString::fromUtf8("Hello 🙂 123") },
			QString::fromUtf8("Hello 🙂 123"));
		Check({}, {});

		auto input = TextWithEntities{
			QString::fromUtf8("🙂 молоко предупреждали"),
			{
				{ EntityType::Bold, 3, 19 },
				{ EntityType::CustomUrl, 3, 6, QString::fromUtf8("https://example.com") },
				{ EntityType::Spoiler, 10, 12 },
			},
		};
		const auto formatted = Check(
			input,
			QString::fromUtf8("🙂 молоко́ предупрежда́ли"));
		Require(formatted.text.entities[0].length() == 21,
			"Bold did not expand with inserted accents");
		Require(formatted.text.entities[1].length() == 7,
			"Link label did not expand");
		Require(formatted.text.entities[1].data() == input.entities[1].data(),
			"Link destination changed");
		Require(formatted.text.entities[2].offset() == 11,
			"Spoiler did not shift after preceding accent");
		Require(input.text == QString::fromUtf8("🙂 молоко предупреждали"),
			"Original message was mutated");

		for (const auto type : { EntityType::Code, EntityType::Pre,
				EntityType::Url, EntityType::Mention, EntityType::Hashtag }) {
			Check({ QString::fromUtf8("молоко предупреждали"), { { type, 0, 6 } } },
				QString::fromUtf8("молоко предупрежда́ли"));
		}
		const auto url = QString::fromUtf8("https://пример.рф/молоко");
		Check({ url + QString::fromUtf8(" молоко"),
			{ { EntityType::Url, 0, int(url.size()) } } },
			url + QString::fromUtf8(" молоко́"));
		Check({ url + QString::fromUtf8(" #молоко молоко") },
			url + QString::fromUtf8(" #молоко молоко́"));
		std::cout << "Stress formatting, preservation, and offset tests passed.\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
