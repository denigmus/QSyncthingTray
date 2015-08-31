// (c) Matthias Frick 2015

#include "syncconnector.h"
#include <QtGui>
#include <QObject>
#include <iostream>

namespace mfk
{
namespace connector
{

SyncConnector::SyncConnector(QUrl url)
{
  mCurrentUrl = url;
  connect(
          &mWebUrl, SIGNAL (finished(QNetworkReply*)),
          this, SLOT (urlTested(QNetworkReply*))
          );
  connect(
          &mWebUrl, SIGNAL (sslErrors(QNetworkReply *, QList<QSslError>)),
          this, SLOT (onSslError(QNetworkReply*))
          );
  
  connect(
          &mHealthUrl, SIGNAL (finished(QNetworkReply*)),
          this, SLOT (connectionHealthReceived(QNetworkReply*))
          );
  connect(
          &mHealthUrl, SIGNAL (sslErrors(QNetworkReply *, QList<QSslError>)),
          this, SLOT (onSslError(QNetworkReply*))
          );
  }

void SyncConnector::setURL(QUrl url, std::string username, std::string password, ConnectionStateCallback setText)
{

  mAuthentication = std::make_pair(username, password);
  url.setUserName(mAuthentication.first.c_str());
  url.setPassword(mAuthentication.second.c_str());
  mCurrentUrl = url;
  url.setPath(tr("/rest/system/version"));
  mConnectionStateCallback = setText;
  QNetworkRequest request(url);
  mWebUrl.clearAccessCache();
  mWebUrl.get(request);
}


void SyncConnector::showWebView()
{
  mpWebView = new QWebView();
  mpWebView->show();
  mpWebView->page()->setNetworkAccessManager(&mWebUrl);
  mpWebView->load(mCurrentUrl);
}

  
void SyncConnector::urlTested(QNetworkReply* reply)
{
  ignoreSslErrors(reply);
  std::string result;
  bool success = false;

  if (reply->error() != QNetworkReply::NoError)
  {
    result = reply->errorString().toStdString();
  }
  else
  {
    QString m_DownloadedData = static_cast<QString>(reply->readAll());
    QJsonDocument replyDoc = QJsonDocument::fromJson(m_DownloadedData.toUtf8());
    QJsonObject replyData = replyDoc.object();
    result = replyData.value(tr("version")).toString().toStdString();
    success = true;
  }
  if (mConnectionStateCallback != nullptr)
  {
    mConnectionStateCallback(result, success);
  }
}


void SyncConnector::checkConnectionHealth()
{
  QUrl requestUrl = mCurrentUrl;
  requestUrl.setPath(tr("/rest/system/connections"));
  QNetworkRequest request(requestUrl);

  mHealthUrl.get(request);
}


void SyncConnector::setConnectionHealthCallback(ConnectionHealthCallback cb)
{
  mConnectionHealthCallback = cb;
  if (connectionHealthTimer)
  {
    connectionHealthTimer->stop();
  }
  connectionHealthTimer = std::shared_ptr<QTimer>(new QTimer(this));
  connect(connectionHealthTimer.get(), SIGNAL(timeout()), this, SLOT(checkConnectionHealth()));
  connectionHealthTimer->start(3000);
}


void SyncConnector::syncThingProcessSpawned(QProcess::ProcessState newState)
{
  if (mProcessSpawnedCallback)
  {
    switch (newState)
    {
      case QProcess::Running:
        mProcessSpawnedCallback(true);
        break;
      case QProcess::NotRunning:
        mProcessSpawnedCallback(false);
        break;
      default:
        mProcessSpawnedCallback(false);
    }
  }
}

  
void SyncConnector::setProcessSpawnedCallback(ProcessSpawnedCallback cb)
{
  mProcessSpawnedCallback = cb;
}


void SyncConnector::connectionHealthReceived(QNetworkReply* reply)
{
  ignoreSslErrors(reply);
  std::map<std::string, std::string> result;
  result.emplace("state", "0");
  if (reply->error() != QNetworkReply::NoError)
  {
  //  std::cout << "Failed: " << reply->error();
  //  result = reply->errorString().toStdString();
  }
  else
  {
    

    if (reply->bytesAvailable() > 0)
    {
      result.clear();
      result.emplace("state", "1");
      QString m_DownloadedData = static_cast<QString>(reply->readAll());
      QJsonDocument replyDoc = QJsonDocument::fromJson(m_DownloadedData.toUtf8());
      QJsonObject replyData = replyDoc.object();
      QJsonObject connectionArray = replyData["connections"].toObject();
      result.emplace("connections", std::to_string(connectionArray.size()));
    }
  }
  if (mConnectionHealthCallback != nullptr)
  {
    mConnectionHealthCallback(result);
  }
}


void SyncConnector::spawnSyncThingProcess(std::string filePath)
{
  mpSyncProcess = new QProcess(this);
  connect(mpSyncProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(syncThingProcessSpawned(QProcess::ProcessState)));
  QString processPath = filePath.c_str();
  QStringList launchArgs;
  launchArgs << "-no-restart";
  launchArgs.append("-no-browser");
  mpSyncProcess->start(processPath, launchArgs);
}


void SyncConnector::ignoreSslErrors(QNetworkReply *reply)
{
  QList<QSslError> errorsThatCanBeIgnored;
  
  errorsThatCanBeIgnored<<QSslError(QSslError::HostNameMismatch);
  errorsThatCanBeIgnored<<QSslError(QSslError::SelfSignedCertificate);
  reply->ignoreSslErrors(errorsThatCanBeIgnored);
}

  
void SyncConnector::onSslError(QNetworkReply* reply)
{
  reply->ignoreSslErrors();
}

  
SyncConnector::~SyncConnector()
{
  mpSyncProcess->kill();
}
  
} // connector
} //mfk