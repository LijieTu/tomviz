/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "DataPropertiesPanel.h"
#include "ui_DataPropertiesPanel.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "pqPropertiesPanel.h"
#include "pqProxyWidget.h"
#include "SetTiltAnglesReaction.h"
#include "Utilities.h"
#include "vtkDataSetAttributes.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "pqView.h"

#include "vtkSMSourceProxy.h"
#include "vtkDataObject.h"
#include "vtkFieldData.h"
#include "vtkDataArray.h"
#include "vtkAlgorithm.h"

#include <QPointer>
#include <QPushButton>
#include <QMainWindow>

namespace tomviz
{

class DataPropertiesPanel::DPPInternals
{
public:
  Ui::DataPropertiesPanel Ui;
  QPointer<DataSource> CurrentDataSource;
  QPointer<pqProxyWidget> ColorMapWidget;
  QPointer<QWidget> TiltAnglesSeparator;

  DPPInternals(QWidget* parent)
  {
    Ui::DataPropertiesPanel& ui = this->Ui;
    ui.setupUi(parent);
    QVBoxLayout* l = ui.verticalLayout;

    l->setSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

    // add separator labels.
    QWidget* separator = pqProxyWidget::newGroupLabelWidget("Filename", parent);
    l->insertWidget(l->indexOf(ui.FileName), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Original Dimensions & Range",
                                                   parent);
    l->insertWidget(l->indexOf(ui.OriginalDataRange), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Transformed Dimensions & Range",
                                                   parent);
    l->insertWidget(l->indexOf(ui.TransformedDataRange), separator);

    this->TiltAnglesSeparator =
      pqProxyWidget::newGroupLabelWidget("Tilt Angles", parent);
    l->insertWidget(l->indexOf(ui.SetTiltAnglesButton), this->TiltAnglesSeparator);

    // set icons for save/restore buttons.
    ui.ColorMapSaveAsDefaults->setIcon(
      ui.ColorMapSaveAsDefaults->style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui.ColorMapRestoreDefaults->setIcon(
      ui.ColorMapRestoreDefaults->style()->standardIcon(QStyle::SP_BrowserReload));

    this->clear();
  }

  void clear()
  {
    Ui::DataPropertiesPanel& ui = this->Ui;
    ui.FileName->setText("");
    ui.OriginalDataRange->setText("");
    ui.OriginalDataType->setText("Type:");
    ui.TransformedDataRange->setText("");
    ui.TransformedDataType->setText("Type:");
    if (this->ColorMapWidget)
    {
      ui.verticalLayout->removeWidget(this->ColorMapWidget);
      delete this->ColorMapWidget;
    }
    this->TiltAnglesSeparator->hide();
    ui.SetTiltAnglesButton->hide();
    ui.TiltAnglesTable->clear();
    ui.TiltAnglesTable->setRowCount(0);
    ui.TiltAnglesTable->hide();
  }

};

//-----------------------------------------------------------------------------
DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : Superclass(parentObject),
  Internals(new DataPropertiesPanel::DPPInternals(this))
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(setDataSource(DataSource*)));
  this->connect(this->Internals->Ui.SetTiltAnglesButton, SIGNAL(clicked()),
                SLOT(setTiltAngles()));
}

//-----------------------------------------------------------------------------
DataPropertiesPanel::~DataPropertiesPanel()
{
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (this->Internals->CurrentDataSource)
  {
    this->disconnect(this->Internals->CurrentDataSource);
  }
  this->Internals->CurrentDataSource = dsource;
  if (dsource)
  {
    this->connect(dsource, SIGNAL(dataChanged()), SLOT(update()),
                  Qt::UniqueConnection);
  }
  this->update();
}

namespace {
QString getDataExtentAndRangeString(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* info = proxy->GetDataInformation(0);

  QString extentString = QString("%1 x %2 x %3")
                          .arg(info->GetExtent()[1] - info->GetExtent()[0] + 1)
                          .arg(info->GetExtent()[3] - info->GetExtent()[2] + 1)
                          .arg(info->GetExtent()[5] - info->GetExtent()[4] + 1);

  if (vtkPVArrayInformation* scalarInfo = tomviz::scalarArrayInformation(proxy))
  {
    return QString("(%1)\t%2 : %3").arg(extentString)
             .arg(scalarInfo->GetComponentRange(0)[0])
             .arg(scalarInfo->GetComponentRange(0)[1]);
  }
  else
  {
    return QString("(%1)\t? : ? (type: ?)").arg(extentString);
  }
}

QString getDataTypeString(vtkSMSourceProxy* proxy)
{
  if (vtkPVArrayInformation* scalarInfo = tomviz::scalarArrayInformation(proxy))
  {
    return QString("Type: %1").arg(vtkImageScalarTypeNameMacro(
                                     scalarInfo->GetDataType()));
  }
  else
  {
    return QString("Type: ?");
  }
}
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::update()
{
  this->disconnect(this->Internals->Ui.TiltAnglesTable,
      SIGNAL(cellChanged(int, int)), this,
      SLOT(onTiltAnglesModified(int, int)));
  this->Internals->clear();

  DataSource* dsource = this->Internals->CurrentDataSource;
  if (!dsource)
  {
    return;
  }
  Ui::DataPropertiesPanel& ui = this->Internals->Ui;
  ui.FileName->setText(dsource->filename());

  ui.OriginalDataRange->setText(getDataExtentAndRangeString(
        dsource->originalDataSource()));
  ui.OriginalDataType->setText(getDataTypeString(
        dsource->originalDataSource()));
  ui.TransformedDataRange->setText(getDataExtentAndRangeString(
        dsource->producer()));
  ui.TransformedDataType->setText(getDataTypeString(
        dsource->producer()));

  pqProxyWidget* colorMapWidget = new pqProxyWidget(dsource->colorMap());
  colorMapWidget->setApplyChangesImmediately(true);
  colorMapWidget->updatePanel();
  ui.verticalLayout->insertWidget(ui.verticalLayout->indexOf(ui.SetTiltAnglesButton)-1,
                                  colorMapWidget);
  colorMapWidget->connect(ui.ColorMapExpander, SIGNAL(toggled(bool)),
                          SLOT(setVisible(bool)));
  colorMapWidget->setVisible(ui.ColorMapExpander->checked());
  colorMapWidget->connect(ui.ColorMapSaveAsDefaults, SIGNAL(clicked()),
                          SLOT(saveAsDefaults()));
  colorMapWidget->connect(ui.ColorMapRestoreDefaults, SIGNAL(clicked()),
                          SLOT(restoreDefaults()));
  this->connect(colorMapWidget, SIGNAL(changeFinished()), SLOT(render()));
  this->Internals->ColorMapWidget = colorMapWidget;

  // display tilt series data
  if (dsource->type() == DataSource::TiltSeries)
  {
    this->Internals->TiltAnglesSeparator->show();
    ui.SetTiltAnglesButton->show();
    ui.TiltAnglesTable->show();
    vtkDataArray* tiltAngles = vtkAlgorithm::SafeDownCast(
        dsource->producer()->GetClientSideObject())
      ->GetOutputDataObject(0)->GetFieldData()->GetArray("tilt_angles");
    ui.TiltAnglesTable->setRowCount(tiltAngles->GetNumberOfTuples());
    ui.TiltAnglesTable->setColumnCount(tiltAngles->GetNumberOfComponents());
    for (int i = 0; i < tiltAngles->GetNumberOfTuples(); ++i)
    {
      double* angles = tiltAngles->GetTuple(i);
      for (int j = 0; j < tiltAngles->GetNumberOfComponents(); ++j)
      {
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setData(Qt::DisplayRole, QString("%1").arg(angles[j]));
        ui.TiltAnglesTable->setItem(i, j, item);
      }
    }
  }
  else
  {
    this->Internals->TiltAnglesSeparator->hide();
    ui.SetTiltAnglesButton->hide();
    ui.TiltAnglesTable->hide();
  }
  this->connect(this->Internals->Ui.TiltAnglesTable,
      SIGNAL(cellChanged(int, int)),
      SLOT(onTiltAnglesModified(int, int)));
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::render()
{
  pqView* view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
  {
    view->render();
  }
  emit colorMapUpdated();
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::onTiltAnglesModified(int row, int column)
{
  DataSource* dsource = this->Internals->CurrentDataSource;
  QTableWidgetItem* item = this->Internals->Ui.TiltAnglesTable->item(row, column);
  QString str = item->data(Qt::DisplayRole).toString();
  if (dsource->type() == DataSource::TiltSeries)
  {
    vtkDataArray* tiltAngles = vtkAlgorithm::SafeDownCast(
        dsource->producer()->GetClientSideObject())
      ->GetOutputDataObject(0)->GetFieldData()->GetArray("tilt_angles");
    bool ok;
    double value = str.toDouble(&ok);
    if (ok)
    {
      double* tuple = tiltAngles->GetTuple(row);
      tuple[column] = value;
      tiltAngles->SetTuple(row, tuple);
    }
    else
    {
      std::cerr << "Invalid tilt angle: " << str.toStdString() << std::endl;
    }
  }
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::setTiltAngles()
{
  DataSource* dsource = this->Internals->CurrentDataSource;
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(this->window());
  SetTiltAnglesReaction::showSetTiltAnglesUI(mainWindow, dsource);
}

//-----------------------------------------------------------------------------
}
