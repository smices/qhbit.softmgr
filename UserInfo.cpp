#include "UserInfo.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QCryptographicHash>
#include "swmgrapp.h"
#include "ConfOperation.h"
#include "curl/curl.h"

QString userinfoItem[userinfoItemCount] = {
	"address",
	"cents",
	"ctime",
	"email",
	"email_valid",

	"gender",
	"id",
	"ltime",
	"mobile",
	"mobile_valid",

	"mtime",
	"nickname",
	"status"
};

UserInfo::UserInfo(QObject *parent) : QObject(parent)
{
	this->init = "0xff";
    _userDatWatcher = NULL;
}

void UserInfo::serializeUserInfo(bool bSerialize) {
	QJsonObject jsUserInfo;
	QByteArray fileBuf;
	QJsonDocument doc;

	QString szFile = ConfOperation::Root().getSubpathFile("Data", "user.dat");
	if (bSerialize) { // write from user.dat
		jsUserInfo["init"] = QJsonValue(this->init);
		jsUserInfo["username"] = QJsonValue(this->username);
		jsUserInfo["password"] = QJsonValue(this->password);
		jsUserInfo["token"] = QJsonValue(this->usertoken);
		for (int i = 0; i < userinfoItemCount; i++) {
			jsUserInfo[userinfoItem[i]] = userPrivateInfo.contains(userinfoItem[i]) ? userPrivateInfo[userinfoItem[i]] : QString("");
		}
		doc = QJsonDocument(jsUserInfo);
		QFile saveFile(szFile);
		if (saveFile.open(QIODevice::ReadWrite)) {
			saveFile.resize(0);
            saveFile.write(doc.toJson(QJsonDocument::Compact));
			saveFile.close();
		}
	}
	else { // read from user.dat
		QFile saveFile(szFile);
		if (saveFile.open(QIODevice::ReadWrite)) {
			fileBuf = saveFile.readAll();
            doc = QJsonDocument::fromJson(fileBuf);
			saveFile.close();
		}
		if ( doc.isEmpty() || !doc.isObject() ) {
			return;
		}
		jsUserInfo = doc.object();
		this->init = jsUserInfo.contains("init") ? jsUserInfo.value("init").toString() : QString("0");
		this->username = jsUserInfo.contains("username") ? jsUserInfo.value("username").toString() : QString("");
		this->password = jsUserInfo.contains("password") ? jsUserInfo.value("password").toString() : QString("");
		this->usertoken = jsUserInfo.contains("token") ? jsUserInfo.value("token").toString() : QString("");
		for (int i = 0; i < userinfoItemCount; i++) {
			userPrivateInfo[userinfoItem[i]] = jsUserInfo.contains(userinfoItem[i]) ? jsUserInfo.value(userinfoItem[i]).toString() : QString("");
		}
	}
}

QJsonObject UserInfo::toJsonObject() {
    QJsonObject userObject;
    userObject["init"] = QJsonValue(init);
    userObject["username"] = QJsonValue(username);
    userObject["password"] = QJsonValue(password);
    userObject["token"] = QJsonValue(usertoken);
    for (int i = 0; i < userinfoItemCount; i++) {
        userObject[userinfoItem[i]] = userPrivateInfo.contains(userinfoItem[i]) ? userPrivateInfo[userinfoItem[i]] : QString("");
    }
    return userObject;
}

void UserInfo::clean() {
    this->init = "0xff";
    this->username.clear();
    this->password.clear();
    this->usertoken.clear();
    for (int i = 0; i < userinfoItemCount; i++) {
        userPrivateInfo[userinfoItem[i]].clear();
    }
}

size_t UserInfo::LoginCallback(char *buffer, size_t size, size_t nitems, void *outstream) {
	QByteArray *response = (QByteArray *)outstream;
	if (!response) {
		return 0;
	}
	response->append(buffer, size*nitems);
	return size * nitems;
}

QString UserInfo::postMethod(std::string url,std::string cookieFile,std::string post) {
	QByteArray response;
	QString szRet;
	CURL* curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.data());
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.data());
		curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookieFile.data());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post.size());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data());

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, UserInfo::LoginCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		curl_easy_setopt(curl, CURLOPT_POST, (long)1);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res == CURLE_OK) {
			QJsonParseError err;
			QJsonDocument doc = QJsonDocument::fromJson(response, &err);
			if (err.error == QJsonParseError::NoError) {
				szRet = doc.toJson(QJsonDocument::Compact);
			}
			else {
				szRet.append("param error");
			}
		}
		else {
			szRet.append("server error");
		}
	}
	else {
		szRet.append("please check network");
	}
	return szRet;
}

QByteArray UserInfo::cryptPassword(QString szPassword) {
	QByteArray byCrypto = szPassword.toUtf8();
	byCrypto = QCryptographicHash::hash(byCrypto, QCryptographicHash::Md5).toHex();
	byCrypto += byCrypto.mid(8, 8);
	byCrypto = QCryptographicHash::hash(byCrypto, QCryptographicHash::Sha1).toHex();
	return byCrypto;
}

void UserInfo::UserLogin(QString szUserName, QString szPassword) {
	QByteArray byCrypto;
	QString POSTFIELDS;
	QString szResult;
	QString szCookieFile = SwmgrApp::GetCookieFile();
	QString szUserLoginUrl = SwmgrApp::GetUserLoginUrl();

	if (szUserName.size()==0) {
		return ;
	}
    clean();
	byCrypto = UserInfo::cryptPassword(szPassword);

	this->username = szUserName;
	this->init = "0";

	POSTFIELDS=QString("%1=%2&%3=%4").arg("username").arg(this->username).arg("password").arg(QString(byCrypto));
	szResult = postMethod(szUserLoginUrl.toStdString(), szCookieFile.toStdString(), POSTFIELDS.toStdString());
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(szResult.toUtf8(), &err);
	if (err.error == QJsonParseError::NoError && doc.isObject()) {
		QJsonObject resObject = doc.object();
		int nCode = resObject.value("code").toInt();
		if (nCode == 0) {
			qDebug() << L"�ɹ���¼";
			resObject = resObject.value("msg").toObject();
			this->init = "2";
			this->usertoken = resObject.value("token").toString();
			this->password = byCrypto;
			for (int i = 0; i < userinfoItemCount; i++) {
				this->userPrivateInfo[userinfoItem[i]] = resObject.contains(userinfoItem[i]) && resObject.value(userinfoItem[i]).isString() ? resObject.value(userinfoItem[i]).toString() : QString("");
			}
		}
		else {
			qDebug() << resObject.value("msg").toString();
		}
	}
	else {
		qDebug() << szResult;
	}
    StopWatcher();
	this->serializeUserInfo(true);
    emit signalLoginUser(toJsonObject().toVariantMap());
    StartWatcher();
}

void UserInfo::RegistUser(QString szUserName, QString szPassword, QString szEmail) {
	QByteArray byCrypto;
	QString POSTFIELDS;
	QString szResult;
	QString szCookieFile = SwmgrApp::GetCookieFile();
	QString szUserRegisteUrl = SwmgrApp::GetUserRegisteUrl();
	if (szUserName.size()==0){
		return;
	}
    clean();

	byCrypto = UserInfo::cryptPassword(szPassword);
	POSTFIELDS = QString("%1=%2&%3=%4&%5=%6").arg("username").arg(szUserName).arg("password").arg(QString(byCrypto)).arg("email").arg(szEmail);
	szResult = postMethod(szUserRegisteUrl.toStdString(), szCookieFile.toStdString(), POSTFIELDS.toStdString());
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(szResult.toUtf8(), &err);
	if (err.error == QJsonParseError::NoError && doc.isObject()) {
		QJsonObject resObject = doc.object();
		int nCode = resObject.value("code").toInt();
		if (nCode == 0) {
			qDebug() << L"ע��ɹ�";
			this->init = "0";
			this->username = szUserName;
		}
		else {
			qDebug() << resObject.value("msg").toString();
            this->init="1";
            this->username = szUserName;
		}
	}
	else {
		qDebug() << szResult;
	}
    StopWatcher();
    this->serializeUserInfo(true);
    emit signalRegisteUser(toJsonObject().toVariantMap());
    StartWatcher();
}

void UserInfo::ModifyUserInfo(QVariantMap userInfo) {
    Q_UNUSED(userInfo);
    emit signalModifyUserInfo(toJsonObject().toVariantMap());
}

void UserInfo::QueryUserInfo() {
    emit signalLoginUser(toJsonObject().toVariantMap());
}

void UserInfo::StartWatcher() {
    if (_userDatWatcher!=NULL) {
        return;
    }
    _userDatWatcher = new QFileSystemWatcher(QStringList()<<ConfOperation::Root().getSubpathFile("Data", "user.dat"),this);
    QObject::connect(_userDatWatcher,SIGNAL(fileChanged(QString)),this,SLOT(ChangedDat(QString)));
}

void UserInfo::StopWatcher() {
    if (_userDatWatcher==NULL) {
        return;
    }
    QObject::disconnect(_userDatWatcher,SIGNAL(fileChanged(QString)),this,SLOT(ChangedDat(QString)));
    delete _userDatWatcher;
    _userDatWatcher = NULL;
}

void UserInfo::ChangedDat(QString path) {
	if (path.compare(ConfOperation::Root().getSubpathFile("Data", "user.dat"), Qt::CaseInsensitive) == 0) {
		qDebug() << path;
		serializeUserInfo();
        QueryUserInfo();
	}
}
