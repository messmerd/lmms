/*
 * AutomatableModelView.h - provides AutomatableModelView base class and
 * provides BoolModelView, FloatModelView, IntModelView subclasses.
 *
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_GUI_AUTOMATABLE_MODEL_VIEW_H
#define LMMS_GUI_AUTOMATABLE_MODEL_VIEW_H

#include "ModelView.h"
#include "AutomatableModel.h"

#include <optional>

class QMenu;
class QMouseEvent;

namespace lmms::gui
{

class LMMS_EXPORT AutomatableModelView : public ModelView
{
public:
	AutomatableModelView( Model* model, QWidget* _this );
	~AutomatableModelView() override = default;

	// some basic functions for convenience
	AutomatableModel* modelUntyped()
	{
		return castModel<AutomatableModel>();
	}

	const AutomatableModel* modelUntyped() const
	{
		return castModel<AutomatableModel>();
	}

	void setModel( Model* model, bool isOldModelValid = true ) override;
	void unsetModel() override;

	template<typename T>
	inline T value() const
	{
		return modelUntyped() ? modelUntyped()->value<T>() : 0;
	}

	inline void setDescription( const QString& desc )
	{
		m_description = desc.trimmed();
	}

	virtual void setUnit(const QString& unit)
	{
		m_unit = unit;
	}

	void addDefaultActions( QMenu* menu );

	float getConversionFactor();


protected:
	virtual void mousePressEvent( QMouseEvent* event );

	/**
	 * @brief Converts a raw model value to text.
	 *
	 * The default implementation is @a defaultValueToText().
	 *
	 * @param internalValue any raw model value within the valid min/max range
	 * @returns the value converted to a string, or std::nullopt if the conversion could not be performed.
	 *
	 * @note Some VSTs may not support values other than the model's current value without resorting to hacks.
	 * @note The caller should use a straightforward float-to-string conversion if std::nullopt is returned.
	 */
	virtual auto valueToText(float internalValue) -> std::optional<QString>;

	//! Straightforward float-to-string conversion (i.e. 1.23 --> "1.23") which never fails.
	static auto defaultValueToText(float internalValue) -> QString;

	/**
	 * @brief Converts the current model value to text.
	 * @returns the current value converted to a string
	 */
	virtual auto currentValueToText() -> QString;

	/**
	 * @brief Converts the current model value to text.
	 *
	 * Unlike @a currentValueToText(), this method is called frequently for update purposes.
	 * Override this method to implement rate-limited updates.
	 *
	 * @returns the current value converted to a string, or std::nullopt to indicate
	 *          the previous text value should continue being used
	 */
	virtual auto currentValueToTextUpdate() -> std::optional<QString>;

	/**
	 * @brief The inverse of @a valueToText().
	 *
	 * The default implementation is a straightforward string-to-float conversion, i.e. "1.23" --> 1.23.
	 *
	 * @returns the value text converted to a float, or std::nullopt if the conversion could not be performed
	 */
	virtual auto textToValue(const QString& text) -> std::optional<float>;

	/**
	 * @brief Provides a string to be displayed to the user in floating text or elsewhere.
	 *
	 * By default the following format is used:
	 *     "[description] [text][unit]"
	 *
	 * @param text the string returned by @a valueToText() or @a currentValueToText()
	 * @returns string for dynamic floating text to display
	 */
	virtual auto getDisplayText(const QString& text) -> QString;

private:
	QString m_description;
	QString m_unit;
//	float m_conversionFactor; // Factor to be applied when the m_model->value is displayed TODO: add better way of converting b/w internal and external values
};




class AutomatableModelViewSlots : public QObject
{
	Q_OBJECT
public:
	AutomatableModelViewSlots( AutomatableModelView* amv, QObject* parent );

public slots:
	void execConnectionDialog();
	void removeConnection();

private slots:
	/// Copy the model's value to the clipboard.
	void copyToClipboard();
	/// Paste the model's value from the clipboard.
	void pasteFromClipboard();

protected:
	AutomatableModelView* m_amv;

} ;



template <typename ModelType> class LMMS_EXPORT TypedModelView : public AutomatableModelView
{
public:
	TypedModelView( Model* model, QWidget* _this) :
		AutomatableModelView( model, _this )
	{}

	ModelType* model()
	{
		return castModel<ModelType>();
	}
	const ModelType* model() const
	{
		return castModel<ModelType>();
	}

	auto currentValueToText() -> QString override
	{
		if constexpr (std::is_same_v<ModelType, FloatModel>)
		{
			if (const auto* model = this->model())
			{
				auto text = this->valueToText(model->getRoundedValue());
				if (!text) { throw std::runtime_error{"Need to override AutomatableModelView::currentValueToText()"}; }
				return *text;
			}
			return QString{};
		}
		else
		{
			return AutomatableModelView::currentValueToText();
		}
	}
};

using FloatModelView = TypedModelView<FloatModel>;
using IntModelView = TypedModelView<IntModel>;
using BoolModelView = TypedModelView<BoolModel>;

} // namespace lmms::gui

#endif // LMMS_GUI_AUTOMATABLE_MODEL_VIEW_H
