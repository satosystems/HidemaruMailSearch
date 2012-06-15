#ifndef ___INIFILE_H
#define ___INIFILE_H

#include <windows.h>
#include <string>

#ifdef __cpluspluc
extern "C" {
#endif

/**
 * 設定ファイルへの読み書きを提供するクラス。
 */
class IniFile {
public:
	IniFile(LPCTSTR iniPath) : m_filePath(iniPath) {}
	~IniFile() {}
	char *read(LPCTSTR section, LPCTSTR key = NULL, LPCTSTR defaultValue = NULL) {
		LPTSTR buf, token;
		DWORD size;
		DWORD rc;
		
		if (section == NULL) {
			return NULL;
		}

		size = 256;
		buf = (LPTSTR)malloc(size);
retry:
		if (buf != NULL) {
			rc	= GetPrivateProfileString(
				section,		// セクション名
				key,			// キー名
				defaultValue,	// 既定の文字列
				buf,			// 情報が格納されるバッファ
				size,			// 情報バッファのサイズ
				m_filePath.c_str()// .ini ファイルの名前
			);

			if (rc >= size -2) {
				size *= 2;
				buf = (LPTSTR)realloc(buf, size);
				goto retry;
			}
			if (buf) {
				if (key == NULL) {
					token = buf;
					// すべてのキーをこのクラスに記憶する
					while (rc) {
						m_keys.push_back(std::string(token));
						DWORD len = (DWORD)strlen(token);
						token += len + 1;
						rc -= len + 1;
					}
					free(buf);
					buf = NULL;
				}
			}
			return buf;
		}
		return NULL;
	}

	std::vector<std::string> keys(LPCSTR section) {
		read(section);
		return m_keys;
	}


	BOOL write(LPCTSTR section, LPCTSTR key, LPCTSTR value) {
		char *val = NULL;
		BOOL mustDelete = false;
		if (value != NULL) {
			// ダブルコーテーション対応
			std::string s = value;
			if (s.size() >= 2 && s[0] == '"' && s[s.size() - 1] == '"') {
				std::string temp = "\"" + s + "\"";
				size_t size = strlen(temp.c_str()) + 1;
				val = new char[size];
				strcpy_s(val, size, temp.c_str());
				mustDelete = true;
			} else {
				val = (char *)value;
			}
		}
		BOOL rc = WritePrivateProfileString(
			section,		// セクション名
			key,			// キー名
			val,			// 追加するべき文字列
			m_filePath.c_str()// .ini ファイル
		);
		if (mustDelete) {
			delete val;
		}
		return rc;

	}

	BOOL del() {
		return DeleteFile(m_filePath.c_str());
	}

	const char *getFilePath() {
		return m_filePath.c_str();
	}

private:
	std::string					m_filePath;		///< INIファイルのパス名。
	std::vector<std::string>	m_keys;			///< 特定のセクションに対するすべてのキー。
	
};


#ifdef __cpluscplus
}
#endif

#endif
