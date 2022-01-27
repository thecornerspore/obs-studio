/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <QMessageBox>
#include "window-basic-main.hpp"
#include "window-basic-source-select.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"

struct AddSourceData {
	obs_source_t *source;
	bool visible;
	obs_transform_info *transform = nullptr;
	obs_sceneitem_crop *crop = nullptr;
	obs_blending_type *blend = nullptr;
};

bool OBSBasicSourceSelect::EnumSources(void *data, obs_source_t *source)
{
	if (obs_source_is_hidden(source))
		return false;

	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_unversioned_id(source);

	if (strcmp(id, window->id) == 0)
		window->ui->sourceList->addItem(QT_UTF8(name));

	return true;
}

bool OBSBasicSourceSelect::EnumGroups(void *data, obs_source_t *source)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_unversioned_id(source);

	if (strcmp(id, window->id) == 0) {
		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		OBSScene scene = main->GetCurrentScene();

		obs_sceneitem_t *existing = obs_scene_get_group(scene, name);
		if (!existing)
			window->ui->sourceList->addItem(QT_UTF8(name));
	}

	return true;
}

void OBSBasicSourceSelect::OBSSourceAdded(void *data, calldata_t *calldata)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceAdded",
				  Q_ARG(OBSSource, source));
}

void OBSBasicSourceSelect::OBSSourceRemoved(void *data, calldata_t *calldata)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceRemoved",
				  Q_ARG(OBSSource, source));
}

void OBSBasicSourceSelect::SourceAdded(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	const char *sourceId = obs_source_get_unversioned_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	ui->sourceList->addItem(name);
}

void OBSBasicSourceSelect::SourceRemoved(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	const char *sourceId = obs_source_get_unversioned_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	QList<QListWidgetItem *> items =
		ui->sourceList->findItems(name, Qt::MatchFixedString);

	if (!items.count())
		return;

	delete items[0];
}

static void AddSource(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);

	if (data->transform != nullptr)
		obs_sceneitem_set_info(sceneitem, data->transform);
	if (data->crop != nullptr)
		obs_sceneitem_set_crop(sceneitem, data->crop);
	if (data->blend != nullptr)
		obs_sceneitem_set_blending_mode(sceneitem, *data->blend);

	obs_sceneitem_set_visible(sceneitem, data->visible);
}

static char *get_new_source_name(const char *name)
{
	struct dstr new_name = {0};
	int inc = 0;

	dstr_copy(&new_name, name);

	for (;;) {
		OBSSourceAutoRelease existing_source =
			obs_get_source_by_name(new_name.array);
		if (!existing_source)
			break;

		dstr_printf(&new_name, "%s %d", name, ++inc + 1);
	}

	return new_name.array;
}

static void AddExisting(OBSSource source, bool visible, bool duplicate,
			obs_transform_info *transform, obs_sceneitem_crop *crop,
			obs_blending_type *blend)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

	if (duplicate) {
		OBSSource from = source;
		char *new_name =
			get_new_source_name(obs_source_get_name(source));
		source = obs_source_duplicate(from, new_name, false);
		bfree(new_name);

		if (!source)
			return;
	}

	AddSourceData data;
	data.source = source;
	data.visible = visible;
	data.transform = transform;
	data.crop = crop;
	data.blend = blend;

	obs_enter_graphics();
	obs_scene_atomic_update(scene, AddSource, &data);
	obs_leave_graphics();
}

static void AddExisting(const char *name, bool visible, bool duplicate,
			obs_transform_info *transform, obs_sceneitem_crop *crop,
			obs_blending_type *blend)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(name);
	if (source) {
		AddExisting(source.Get(), visible, duplicate, transform, crop,
			    blend);
	}
}

bool AddNew(QWidget *parent, const char *id, const char *name,
	    const bool visible, OBSSource &newSource)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	bool success = false;
	if (!scene)
		return false;

	OBSSourceAutoRelease source = obs_get_source_by_name(name);
	if (source && parent) {
		OBSMessageBox::information(parent, QTStr("NameExists.Title"),
					   QTStr("NameExists.Text"));

	} else {
		const char *v_id = obs_get_latest_input_type_id(id);
		source = obs_source_create(v_id, name, NULL, nullptr);

		if (source) {
			AddSourceData data;
			data.source = source;
			data.visible = visible;

			obs_enter_graphics();
			obs_scene_atomic_update(scene, AddSource, &data);
			obs_leave_graphics();

			newSource = source;

			/* set monitoring if source monitors by default */
			uint32_t flags = obs_source_get_output_flags(source);
			if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
				obs_source_set_monitoring_type(
					source,
					OBS_MONITORING_TYPE_MONITOR_ONLY);
			}

			success = true;
		}
	}

	return success;
}

void OBSBasicSourceSelect::on_buttonBox_accepted()
{
	bool useExisting = ui->selectExisting->isChecked();
	bool visible = ui->sourceVisible->isChecked();

	if (useExisting) {
		QListWidgetItem *item = ui->sourceList->currentItem();
		if (!item)
			return;

		QString source_name = item->text();
		AddExisting(QT_TO_UTF8(source_name), visible, false, nullptr,
			    nullptr, nullptr);

		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		const char *scene_name =
			obs_source_get_name(main->GetCurrentSceneSource());

		auto undo = [scene_name, main](const std::string &data) {
			UNUSED_PARAMETER(data);
			OBSSourceAutoRelease scene_source =
				obs_get_source_by_name(scene_name);
			main->SetCurrentScene(scene_source, true);

			OBSSceneAutoRelease scene =
				obs_get_scene_by_name(scene_name);
			OBSSceneItem item;
			auto cb = [](obs_scene_t *scene,
				     obs_sceneitem_t *sceneitem, void *data) {
				UNUSED_PARAMETER(scene);
				OBSSceneItem &last =
					*reinterpret_cast<OBSSceneItem *>(data);
				last = sceneitem;
				return true;
			};
			obs_scene_enum_items(scene, cb, &item);

			obs_sceneitem_remove(item);
		};

		auto redo = [scene_name, main, source_name,
			     visible](const std::string &data) {
			UNUSED_PARAMETER(data);
			OBSSourceAutoRelease scene_source =
				obs_get_source_by_name(scene_name);
			main->SetCurrentScene(scene_source, true);
			AddExisting(QT_TO_UTF8(source_name), visible, false,
				    nullptr, nullptr, nullptr);
		};

		undo_s.add_action(QTStr("Undo.Add").arg(source_name), undo,
				  redo, "", "");
	} else {
		if (ui->sourceName->text().isEmpty()) {
			OBSMessageBox::warning(this,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
			return;
		}

		if (!AddNew(this, id, QT_TO_UTF8(ui->sourceName->text()),
			    visible, newSource))
			return;

		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		std::string scene_name =
			obs_source_get_name(main->GetCurrentSceneSource());
		auto undo = [scene_name, main](const std::string &data) {
			OBSSourceAutoRelease source =
				obs_get_source_by_name(data.c_str());
			obs_source_remove(source);

			OBSSourceAutoRelease scene_source =
				obs_get_source_by_name(scene_name.c_str());
			main->SetCurrentScene(scene_source.Get(), true);
		};
		OBSDataAutoRelease wrapper = obs_data_create();
		obs_data_set_string(wrapper, "id", id);
		OBSSceneItemAutoRelease item = obs_scene_sceneitem_from_source(
			main->GetCurrentScene(), newSource);
		obs_data_set_int(wrapper, "item_id",
				 obs_sceneitem_get_id(item));
		obs_data_set_string(
			wrapper, "name",
			ui->sourceName->text().toUtf8().constData());
		obs_data_set_bool(wrapper, "visible", visible);

		auto redo = [scene_name, main](const std::string &data) {
			OBSSourceAutoRelease scene_source =
				obs_get_source_by_name(scene_name.c_str());
			main->SetCurrentScene(scene_source.Get(), true);

			OBSDataAutoRelease dat =
				obs_data_create_from_json(data.c_str());
			OBSSource source;
			AddNew(NULL, obs_data_get_string(dat, "id"),
			       obs_data_get_string(dat, "name"),
			       obs_data_get_bool(dat, "visible"), source);
			OBSSceneItemAutoRelease item =
				obs_scene_sceneitem_from_source(
					main->GetCurrentScene(), source);
			obs_sceneitem_set_id(item, (int64_t)obs_data_get_int(
							   dat, "item_id"));
		};
		undo_s.add_action(QTStr("Undo.Add").arg(ui->sourceName->text()),
				  undo, redo,
				  std::string(obs_source_get_name(newSource)),
				  std::string(obs_data_get_json(wrapper)));
	}

	done(DialogCode::Accepted);
}

void OBSBasicSourceSelect::on_buttonBox_rejected()
{
	done(DialogCode::Rejected);
}

static inline const char *GetSourceDisplayName(const char *id)
{
	if (strcmp(id, "scene") == 0)
		return Str("Basic.Scene");
	const char *v_id = obs_get_latest_input_type_id(id);
	return obs_source_get_display_name(v_id);
}

Q_DECLARE_METATYPE(OBSScene);

template<typename T> static inline T GetOBSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::OBSRef)).value<T>();
}

OBSBasicSourceSelect::OBSBasicSourceSelect(OBSBasic *parent, const char *id_,
					   undo_stack &undo_s)
	: QDialog(parent),
	  ui(new Ui::OBSBasicSourceSelect),
	  id(id_),
	  undo_s(undo_s)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->sourceList->setAttribute(Qt::WA_MacShowFocusRect, false);

	QString placeHolderText{QT_UTF8(GetSourceDisplayName(id))};

	QString text{placeHolderText};
	int i = 2;
	OBSSourceAutoRelease source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(text)))) {
		text = QString("%1 %2").arg(placeHolderText).arg(i++);
	}

	ui->sourceName->setText(text);
	ui->sourceName->setFocus(); //Fixes deselect of text.
	ui->sourceName->selectAll();

	installEventFilter(CreateShortcutFilter());

	if (strcmp(id_, "scene") == 0) {
		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		OBSSource curSceneSource = main->GetCurrentSceneSource();

		ui->selectExisting->setChecked(true);
		ui->createNew->setChecked(false);
		ui->createNew->setEnabled(false);
		ui->sourceName->setEnabled(false);

		int count = main->ui->scenes->count();
		for (int i = 0; i < count; i++) {
			QListWidgetItem *item = main->ui->scenes->item(i);
			OBSScene scene = GetOBSRef<OBSScene>(item);
			OBSSource sceneSource = obs_scene_get_source(scene);

			if (curSceneSource == sceneSource)
				continue;

			const char *name = obs_source_get_name(sceneSource);
			ui->sourceList->addItem(QT_UTF8(name));
		}
	} else if (strcmp(id_, "group") == 0) {
		obs_enum_sources(EnumGroups, this);
	} else {
		obs_enum_sources(EnumSources, this);
	}
}

void OBSBasicSourceSelect::SourcePaste(SourceCopyInfo &info, bool dup)
{
	OBSSource source = OBSGetStrongRef(info.weak_source);
	if (!source)
		return;

	AddExisting(source, info.visible, dup, &info.transform, &info.crop,
		    &info.blend);
}
