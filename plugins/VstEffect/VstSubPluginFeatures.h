/*
 * VstSubPluginFeatures.h - derivation from
 *                          Plugin::Descriptor::SubPluginFeatures for
 *                          hosting VST-plugins
 *
 * Copyright (c) 2006-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef _VST_SUBPLUGIN_FEATURES_H
#define _VST_SUBPLUGIN_FEATURES_H


#include "Plugin.h"

namespace lmms
{


class VstSubPluginFeatures : public Plugin::Descriptor::SubPluginFeatures
{
public:
	VstSubPluginFeatures( Plugin::Type _type );

	void fillDescriptionWidget( QWidget * _parent,
											const Key * _key ) const override;

	void listSubPluginKeys( const Plugin::Descriptor * _desc,
											KeyList & _kl ) const override;
private:
	void addPluginsFromDir(QStringList* filenames,  QString path) const;
} ;


} // namespace lmms

#endif

