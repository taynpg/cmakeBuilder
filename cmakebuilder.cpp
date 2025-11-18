#include "cmakebuilder.h"
#include "./ui_cmakebuilder.h"

CmakeBuilder::CmakeBuilder(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CmakeBuilder)
{
    ui->setupUi(this);
}

CmakeBuilder::~CmakeBuilder()
{
    delete ui;
}

