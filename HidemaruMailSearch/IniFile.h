#ifndef ___INIFILE_H
#define ___INIFILE_H

#include <windows.h>
#include <string>

#ifdef __cpluspluc
extern "C" {
#endif

/**
 * �ݒ�t�@�C���ւ̓ǂݏ�����񋟂���N���X�B
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
				section,		// �Z�N�V������
				key,			// �L�[��
				defaultValue,	// ����̕�����
				buf,			// ��񂪊i�[�����o�b�t�@
				size,			// ���o�b�t�@�̃T�C�Y
				m_filePath.c_str()// .ini �t�@�C���̖��O
			);

			if (rc >= size -2) {
				size *= 2;
				buf = (LPTSTR)realloc(buf, size);
				goto retry;
			}
			if (buf) {
				if (key == NULL) {
					token = buf;
					// ���ׂẴL�[�����̃N���X�ɋL������
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
			// �_�u���R�[�e�[�V�����Ή�
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
			section,		// �Z�N�V������
			key,			// �L�[��
			val,			// �ǉ�����ׂ�������
			m_filePath.c_str()// .ini �t�@�C��
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
	std::string					m_filePath;		///< INI�t�@�C���̃p�X���B
	std::vector<std::string>	m_keys;			///< ����̃Z�N�V�����ɑ΂��邷�ׂẴL�[�B
	
};


#ifdef __cpluscplus
}
#endif

#endif
