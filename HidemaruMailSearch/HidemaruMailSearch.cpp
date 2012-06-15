#define EXPORT extern "C" __declspec(dllexport) 

#pragma warning(disable:4312) // 32bit ���� 64bit �ւ̃L���X�g���̌x���𖳎�
#pragma warning(disable:4100) // ��x���g�p���Ȃ������̌x���𖳎�
#pragma warning(disable:4706) // �����������ł̒l�̑���̌x���𖳎�

// �e���`
#define HIDEMARU_MAIL_DIR_KEY "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\TuruKameDir"
#define MAX_HISTORY (20)
#define HELPFILE_LINE "86" // HidemaruMailSearch.txt�̃R�}���h�����J�n�s�̍s�ԍ�

#define WIN32_LEAN_AND_MEAN		// Windows �w�b�_�[����g�p����Ă��Ȃ����������O���܂��B
#include <windows.h>
#include <windowsx.h>
#include <imm.h> // IME �̏�ԕύX
#include <string>
#include <vector>
#include "resource.h"
#include "registory.h"
#include "IniFile.h"
#include "boost/regex.hpp"
#include "boost/tokenizer.hpp"

// �֐��}�N��
#define DEF_FUNC(fn) fn##_ fn;
#define GET_FUNC(fn) fn = (fn##_)GetProcAddress(hInstDLL, #fn);


// TKInfo.dll �̊֐���`
typedef int (*HidemaruMailVersion_)(void);
typedef int (*StartDoGrep_)(const char *str1, const char *str2);
typedef int (*SetFindPack_)(const char *str1);
typedef const char * (*CurrentAccount_)(void);
typedef const char * (*CurrentFolder_)(void);
typedef const char * (*YenEncode_)(const char *str1);
typedef void * (*ExecAtMain_)(const char *str1, ...);
typedef int (*SetQuietMode_)(int num1);
typedef int (*IsHidemaruMail_)(void);
typedef int (*IsHidemaruMailMain_)(void);
typedef int (*IsHidemaruMailGrep_)(void);
typedef int (*MainWnd_)(void);
typedef int (*PushFindPack_)(void);


DEF_FUNC(HidemaruMailVersion);
DEF_FUNC(StartDoGrep);
DEF_FUNC(SetFindPack);
DEF_FUNC(CurrentAccount);
DEF_FUNC(CurrentFolder);
DEF_FUNC(YenEncode);
DEF_FUNC(ExecAtMain);
DEF_FUNC(SetQuietMode);
DEF_FUNC(IsHidemaruMail);
DEF_FUNC(IsHidemaruMailMain);
DEF_FUNC(IsHidemaruMailGrep);
DEF_FUNC(MainWnd);
DEF_FUNC(PushFindPack);

/**
 * ���� DLL ���̂��̂̃C���X�^���X�B
 */
static HINSTANCE hInst = NULL;

/**
 * TkInfo.dll �̃C���X�^���X�B
 */
static HINSTANCE hInstDLL = NULL;

/**
 * �ݒ�t�@�C���𑀍삷��C���X�^���X��ێ�����ϐ��B
 *
 * DLL �� ATTACH �ŏ���������ADLL �� DETACH �ŊJ�������B
 */
static IniFile *iniFile = NULL;

/**
 * ����������̗������i�[����z��B
 * 
 * DLL �� ATTACH �� INI �t�@�C������ǂݏo���ꏉ��������鑼�A
 * OK �{�^���������ɍX�V�����B
 * �i�[���ɏd�����O���D��ō폜�����B
 *
 * �����̃T�C�Y�� MAX_HISTORY �܂łƂ���B
 */
static std::vector<std::string> history;

/**
 * �I������Ă��郉�W�I�{�^���� ID ���i�[����ϐ��B
 *
 * DLL �� ATTACH �� INI �t�@�C������ǂݏo���ꏉ��������鑼�A
 * OK �{�^���������ɍX�V�����B
 */
static int selectedRadioButton;

/**
 * �`�F�b�N�{�b�N�X�̑I����Ԃ��i�[����ϐ��B
 *
 * DLL �� ATTACH �� INI �t�@�C������ǂݏo���ꏉ��������鑼�A
 * OK �{�^���������ɍX�V�����B
 *
 * @li TRUE �`�F�b�N����Ă���
 * @li FALSE �`�F�b�N����Ă��Ȃ�
 */
static int selectedCheckBox;

/**
 * �}�N���N�����Ƀ��[�U���I�����Ă��镶������i�[����ϐ��B
 */
static std::string selectedText;

#ifdef _DEBUG
static FILE *logFile = NULL;

static void println(const char *s) {
	if (s != NULL) {
		fprintf(logFile, "%s", s);
		fprintf(logFile, "\n");
	}
}

static void println(char *s) {
	println((const char *)s);
}

static void println(std::string s) {
	println(s.c_str());
}

static void println(int i) {
	char temp[20] = {0};
	_itoa_s(i, temp, 10);
	println(temp);
}
#endif

enum {
	TARGET_SMALLHEADERBODY,
	TARGET_SUBJECT,
	TARGET_FROM_TO,
	TARGET_FROM,
	TARGET_TO,
	TARGET_BODY,
	TARGET_HEADER,
	TARGET_SMALLHEADER,
	TARGET_ALL,
	TARGET_MEMO,
	TARGET_ATTACH,
	TARGET_FLAG,
//	TARGET_DEBUG,
//	TARGET_HELP,
};

static char *targets[] = {
	"smallheaderbody",
	"subject",
	"from+to",
	"from",
	"to",
	"body",
	"header",
	"smallheader",
	"all",
	"memo",
	"attach",
	"flag",
//	"fav",
//	"debug",
//	"help",
};

static char *colon_targets[] = {
	"smallheaderbody:",
	"subject:",
	"from+to:",
	"from:",
	"to:",
	"body:",
	"header:",
	"smallheader:",
	"all:",
	"memo:",
	"attach:",
	"flag:",
//	"fav:",
//	"debug:",
//	"help:",
};

static char *regex_targets[] = {
	"smallheaderbody:",
	"subject:",
	"from\\+to:",
	"from:",
	"to:",
	"body:",
	"header:",
	"smallheader:",
	"all:",
	"memo:",
	"attach:",
	"flag:",
//	"fav:",
//	"debug:",
//	"help:",
};


/**
 * �w���v��\������R�}���h���C���B
 */
static std::string helpCmd("");

#define	macropath "HKEY_CURRENT_USER\\Software\\Hidemaruo\\Hidemaru\\Env\\MacroPath"
#define	turukamedir "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\TuruKameDir"
#define	hideinstloc "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Hidemaru\\InstallLocation"

/**
 * INI �t�@�C����ۑ�����f�B���N�g���i���̃��W���[�����z�u����Ă���f�B���N�g���j���擾����B
 *
 * ���̊֐��͐�������ƃf�B���N�g�����|�C���^�ŕԂ��B
 * �Ăяo�����͖߂�l�� NULL �ł͂Ȃ��Ȃ�A�|�C���^��K�؂ɊJ�����Ȃ���΂Ȃ�Ȃ��B
 *
 * @return �f�B���N�g���p�X
 */
static char *getMacroDir(void) {
	char *work;
	char *temp;
	DWORD type;
	
	if ((temp = (char *)get_reg_value(hideinstloc, &type)) == NULL) {
		// �G�ۂ��C���X�g�[������Ă��Ȃ��Ȃ�A�G�ۃ��[���f�B���N�g�����z�[��
		work = (char *)get_reg_value(turukamedir, &type);
	} else {
		// �G�ۂ��C���X�g�[������Ă���Ȃ�}�N���p�X�𒲂ׂ�
		if ((work = (char *)get_reg_value(macropath, &type)) == NULL) {
			// �}�N���p�X�����݂��Ȃ���ΏG�ۂ� InstallLocation ���z�[��
			size_t size = strlen(temp) + 1;
			work = (char *)malloc(size);
			strcpy_s(work, size, temp);
		}
	}
	
	if (temp != NULL) {
		free(temp);
	}
	return work;
}

/**
 * �_�C�A���O����ʂ̒����ɔz�u����B
 * @param [in] hwnd �����ɔz�u����_�C�A���O�̃n���h��
 */
static void center_window(HWND hwnd) {
	RECT desktop;
	RECT rect;
	int width, height;
	int x, y;

	GetWindowRect(GetDesktopWindow(), &desktop);
	GetWindowRect(hwnd, &rect);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	x = (desktop.left + desktop.right) / 2 - width / 2;
	y = (desktop.top + desktop.bottom) / 2 - height / 2;

	if (x < desktop.left) {
		x = desktop.left;
	} else if (x + width > desktop.right) {
		x = desktop.right - width;
	}

	if (y < desktop.top) {
		y = desktop.top;
	} else if (y + height > desktop.bottom) {
		y = desktop.bottom - height;
	}

	SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}


/**
 * �_�C�A���O�̃v���V�[�W���B
 * @param [in] hDlg �_�C�A���O�{�b�N�X�̃n���h�����i�[����Ă��܂�
 * @param [in] uMsg ���b�Z�[�W���i�[����Ă��܂�
 * @param [in] wParam ���b�Z�[�W�̒ǉ���񂪊i�[����Ă��܂�
 * @param [in] lParam ���b�Z�[�W�̒ǉ���񂪊i�[����Ă��܂�
 * @return ���b�Z�[�W�����������ꍇ�� 0 �ȊO(TRUE)�A�����łȂ���� 0(FALSE)
 */
static BOOL CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HWND hwndComboBox = NULL;
	static HWND hwndEdit = NULL;
	static LRESULT selectedIndex = CB_ERR;
	static int length = 0;
	char *imeControl = NULL;
	switch (uMsg) {
	case WM_INITDIALOG:
		center_window(hDlg);

		hwndComboBox = GetDlgItem(hDlg, IDC_COMBO1);
		SendMessage(hwndComboBox, CB_LIMITTEXT, 0, 0);
		hwndEdit = GetWindow(hwndComboBox, GW_CHILD);
		SetFocus(hwndComboBox);

		SetFocus(GetDlgItem(hDlg, IDC_COMBO1));

		for (unsigned int i = 0; i < history.size(); i++) {
			SendMessage(hwndComboBox, CB_INSERTSTRING, i, (LPARAM)history[i].c_str());
		}

		CheckRadioButton(hDlg, IDC_RADIO1, IDC_RADIO4, selectedRadioButton);
		CheckDlgButton(hDlg, IDC_CHECK1, selectedCheckBox);
		if (selectedRadioButton == IDC_RADIO1 || selectedRadioButton == IDC_RADIO2) {
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK1), FALSE);
		}

		if (selectedText.size() != 0) {
			SetWindowText(hwndEdit, selectedText.c_str());
		}

		if ((imeControl = iniFile->read("IME", "control")) && strcmp(imeControl, "1") == 0) {
			HIMC hImc;
			DWORD dwConv, dwSent;
			
			hImc = ImmGetContext(hDlg);

			ImmGetConversionStatus(hImc, &dwConv, &dwSent);

			if (ImmGetOpenStatus(hImc)) {
				ImmSetOpenStatus(hImc, false);
			}
			ImmReleaseContext(hDlg, hImc);
		}

		// SetFocus �Ńt�H�[�J�X�ړ����s�����ꍇ�� FALSE ��Ԃ��Ȃ���΂Ȃ�Ȃ�
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
			return TRUE;

		case IDOK:
			if (IsDlgButtonChecked(hDlg, IDC_RADIO1)) {
				selectedRadioButton = IDC_RADIO1;
			} else 	if (IsDlgButtonChecked(hDlg, IDC_RADIO2)) {
				selectedRadioButton = IDC_RADIO2;
			} else 	if (IsDlgButtonChecked(hDlg, IDC_RADIO3)) {
				selectedRadioButton = IDC_RADIO3;
			} else 	if (IsDlgButtonChecked(hDlg, IDC_RADIO4)) {
				selectedRadioButton = IDC_RADIO4;
			}
			selectedCheckBox = IsDlgButtonChecked(hDlg, IDC_CHECK1);

			// �ݒ�t�@�C���ɏ�Ԃ������o��
			switch (selectedRadioButton) {
			case IDC_RADIO1:
				iniFile->write("Config", "radio", "1");
				break;

			case IDC_RADIO2:
				iniFile->write("Config", "radio", "2");
				break;

			case IDC_RADIO3:
				iniFile->write("Config", "radio", "3");
				break;

			case IDC_RADIO4:
				iniFile->write("Config", "radio", "4");
				break;

			default:
				iniFile->write("Config", "radio", "1");
				break;
			}

			if (selectedCheckBox) {
				iniFile->write("Config", "check", "TRUE");
			} else {
				iniFile->write("Config", "check", "FALSE");
			}

			// ������ۑ�
			length = GetWindowTextLength(hwndEdit);

			if (length != 0) {
				char *buf = new char[length + 1];
				buf[length] = '\0';
				GetWindowText(hwndEdit, buf, length + 1);

				std::string temp = buf;

				delete[] buf;
				history.insert(history.begin(), temp);

				for (unsigned int i = 0; i < history.size() - 1; i++) {
					for (unsigned int j = i + 1; j < history.size(); j++) {
						std::string str1 = history[i];
						std::string str2 = history[j];
						if (str1 == str2) {
							history[j] = "";
						}
					}
				}
				int index = 0;
				for (unsigned int i = 0; i < MAX_HISTORY && i < history.size(); i++) {
					if (history[i] != "") {
						std::string key = "history";
						char num[3] = {0};
						sprintf_s(num, "%d", index);
						key += num;

						iniFile->write("History", key.c_str(), history[i].c_str());
						index++;
					}
				}
			}

			if (length != 0) {
	            EndDialog(hDlg, IDOK);
			} else {
				EndDialog(hDlg, IDCANCEL);
			}
			return TRUE;

		case IDC_RADIO1:
		case IDC_RADIO2:
			CheckRadioButton(hDlg, IDC_RADIO1, IDC_RADIO4, LOWORD(wParam));
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK1), FALSE);
			return TRUE;

		case IDC_RADIO3:
		case IDC_RADIO4:
			CheckRadioButton(hDlg, IDC_RADIO1, IDC_RADIO4, LOWORD(wParam));
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK1), TRUE);
			return TRUE;

		case IDC_COMBO1:
			switch (HIWORD(wParam)) {
			case CBN_SELCHANGE: // �I�����ڂ��ύX���ꂽ�Ƃ�
			case CBN_EDITUPDATE: // ���ړ��͂œ��e���ύX���ꂽ�Ƃ��B�ύX���e���\�������O�ɌĂ΂��
			case CBN_EDITCHANGE: // ���ړ��͂œ��e���ύX���ꂽ�Ƃ��B�ύX���e���\�����ꂽ��ɌĂ΂��
				selectedIndex = SendMessage(hwndComboBox, CB_GETCURSEL, 0, 0);
				break;
			}
		
			break;

		default:
			break;
		}
		return FALSE;

	case WM_TIMER:
		return FALSE;

	case WM_CLOSE:
		EndDialog(hDlg, WM_CLOSE);
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

/**
 * ���͕����̑O��̃X�y�[�X���폜����B
 * @param [in,out] inputText �Ώۂ̕�����
 */
static void trim(std::string &inputText) {
	boost::smatch m;
	boost::regex ftrim;
	boost::regex rtrim;
	// �S�p�X�y�[�X���Z�p���[�^�Ƃ��Ĉ����I�v�V����
	if (!strcmp("1", iniFile->read("Option", "fullwidthspace_separator"))) {
		ftrim = boost::regex("^[ �@]+");
		rtrim = boost::regex("[ �@]+$");
	} else {
		ftrim = boost::regex("^ +");
		rtrim = boost::regex(" +$");
	}
	if (boost::regex_search(inputText, m, ftrim)) {
		inputText = boost::regex_replace(inputText, ftrim, "", boost::format_all);
	}
	if (boost::regex_search(inputText, m, rtrim)) {
		inputText = boost::regex_replace(inputText, rtrim, "", boost::format_all);
	}
}

#define QUERY_NONE 0
#define QUERY_FOUND 1
#define QUERY_ATTACH 2
#define QUERY_REGULAR 3
#define QUERY_FLAG 4
#define REGEX_DQUOTE "(?<!\\\\)\"(\\\\.|[^\\\\\"])+\""
#define REGEX_QUERY "([^\" ]{0,1}[^ ]+)"

/**
 * ���������N�G���[�̎�ނ�Ԃ��B
 * @param [in] inputText �����Ώ�
 * @return ���������N�G���̎��
 */
static int findTag(const std::string &inputText) {
	if (inputText.find('"') == 0) {
		return TARGET_SMALLHEADERBODY;
	} else {
		for (int i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
			if (inputText.find(colon_targets[i]) == 0) {
				return i;
			}
		}
		return TARGET_SMALLHEADERBODY;
	}
}

/**
 * �G�ۃ��[���̎d�l�ɂ��킹��������������쐬����B
 *
 * @param [in] tagIndex �����Ώہi"body"�A"header" �Ȃǁj�̃C���f�b�N�X
 * @param [in,out] inputText ���[�U�̓��͕�����
 * @param [in,out] query �쐬��������������
 * @param [in,out] flag �쐬�����t���O������
 * @return ���쌋��
 */
static int makeQuery(int tagIndex, std::string &inputText, std::string &query, std::string &flag) {
	// �Y�t�t�@�C���L�����ŏ��ɒ�������
	if (tagIndex == TARGET_ATTACH) {
		size_t pos = inputText.find("attach:*");
		if (pos != std::string::npos) {
			inputText.erase(pos, strlen("attach:*"));
			if (flag == "") {
				flag = "flag=attach";
			} else {
				flag += "&attach";
			}
			return QUERY_ATTACH;
		}
	}
	// �t���O�`�F�b�N
	if (tagIndex == TARGET_FLAG) {
		inputText.erase(0, strlen(colon_targets[TARGET_FLAG]));
		size_t pos = inputText.find(' ');
		std::string flags("");
		if (pos != std::string::npos) {
			flags = inputText.substr(0, pos);
			inputText.erase(0, pos);
		} else {
			flags = inputText;
			inputText = "";
		}
		if (flag == "") {
			flag = "flag=";
			flag += flags;
		} else {
			flag += "&";
			flag += flags;
		}
		return QUERY_FLAG;
	}

	boost::regex r;
	boost::smatch m;
	int rc = QUERY_NONE;

	std::string regexStr = regex_targets[tagIndex];

	// ���K�\���t���O�`�F�b�N
	regexStr += REGEX_DQUOTE;
	r = boost::regex(regexStr);
	if (boost::regex_search(inputText, m, r)) {
		rc = QUERY_REGULAR;
	}
	if (rc != QUERY_REGULAR) {
		regexStr = regex_targets[tagIndex];
		regexStr += REGEX_QUERY;
		r = boost::regex(regexStr);
	}
	if (rc == QUERY_REGULAR || boost::regex_search(inputText, m, r)) {
		std::string s = m.str();
		std::string deleteTarget = m.str();
		s = s.substr(strlen(targets[tagIndex]) + 1);
		if (rc != QUERY_REGULAR) {
			rc = QUERY_FOUND;
		}
		if (query.size() != 0 && query[query.size() - 1] == ')') {
			query += "and";
		}
		if (rc == QUERY_REGULAR) {
			query += "(";
			query += s.c_str();
		} else {
			// �S�p�X�y�[�X���Z�p���[�^�Ƃ��Ĉ����I�v�V����
			if (!strcmp("1", iniFile->read("Option", "fullwidthspace_separator"))) {
				size_t i = 0;
				if ((i = s.find("�@", i)) != std::string::npos) {
					s.replace(i, 2, " ");
					s.erase(i + 1);
					deleteTarget.erase(strlen(targets[tagIndex]) + 1 + i + 2);
				}
			}

			query += "(\"";
			query += YenEncode(s.c_str());
			query += "\"";
		}
		if (rc == QUERY_REGULAR) {
			query += ",regular";
		}
		if (tagIndex == TARGET_ATTACH) {
			query += ",target=\"X-Attach\")";
		} else {
			query += ",target=";
			query += targets[tagIndex];
			query += ")";
		}
		//inputText = boost::regex_replace(inputText, r, "", boost::format_all);  // �� ����ł������A���Č������������R�����g�A�E�g���Ȃ��H
		inputText.erase(inputText.find(deleteTarget), deleteTarget.size());
	} else {
		inputText = colon_targets[TARGET_SMALLHEADERBODY] + inputText;
	}
	return rc;
}

/**
 * �}�N������Ăяo�����G���g���|�C���g�B
 * @param [in] text ����������
 * @return �߂�l�ɈӖ��͂Ȃ�
 */
EXPORT int search(const char *text) {
	if (!IsHidemaruMail()) {
#ifndef _DEBUG
		MessageBox(NULL, "���̃}�N���͏G�ۂ���͋N���ł��܂���B", NULL, MB_OK);
		return 0;
#endif
	}
	HWND hwnd = reinterpret_cast<HWND>(MainWnd());

	if (hwnd == NULL) {
		MessageBox(NULL, "�G�ۃ��[���{�̂��N�����ĂȂ���΂Ȃ�܂���B", NULL, MB_OK);
		return 0;
	}

	if (strlen(text) != 0) {
		std::string oneLine = text;
		size_t pos;
		if ((pos = oneLine.find("\r\n")) != std::string.npos) {
			oneLine[pos] = '\0';
		}
		if (oneLine.find(' ') != std::string.npos) {
			selectedText = "\"";
			selectedText += YenEncode(oneLine.c_str());
			selectedText += "\"";
		} else {
			selectedText = YenEncode(oneLine.c_str());
		}
	}

// SetQuietMode ���g�p����ƁA�������ʃE�B���h�E����O�ɗ��Ȃ�
//	SetQuietMode(1);
	int rc = (int)DialogBox(hInst, (LPCTSTR)IDD_HIDEMARUMAILSEARCH_DIALOG, hwnd, (DLGPROC)DialogProc);
//	SetQuietMode(0);

	if (rc == IDOK) {
		const char *account = NULL;
		const char *folder = NULL;

		switch (selectedRadioButton) {
		case IDC_RADIO1:
		case IDC_RADIO2:
			account = (const char *)ExecAtMain("CurrentAccount");
			if (IsHidemaruMailMain() || IsHidemaruMailGrep()) {
				folder = CurrentFolder();
			} else {
				folder = (const char *)ExecAtMain("CurrentFolder");
			}
			break;

		case IDC_RADIO3:
			account = (const char *)ExecAtMain("CurrentAccount");
			if (selectedCheckBox) {
				folder = "";
			} else {
				folder = "��M+���M�ς�+���[�U�[";
			}
			break;

		case IDC_RADIO4:
			account = "";
			if (selectedCheckBox) {
				folder = "";
			} else {
				folder = "��M+���M�ς�+���[�U�[";
			}
			break;
		}

#ifdef _DEBUG
switch (selectedRadioButton) {
case IDC_RADIO1:
case IDC_RADIO2:
case IDC_RADIO3:
	if (account == NULL || strlen(account) == 0) {
		MessageBox(NULL, "�G�ۃ��[���ɃA�J�E���g��������܂���ł����B\n���̃G���[���p������悤�Ȃ��҂ɘA�����Ă��������B", "HidemaruMailSearch", MB_OK);
	}
	break;
}
#endif

		std::string inputText = history[0].c_str();
		std::string query;
		trim(inputText);

		// ���C�ɓ���̕ۑ��Ɠǂݍ���
		if (inputText.find("fav:") == 0) {
			size_t pos = inputText.find(' ');
			if (pos != std::string::npos) {
				size_t len = strlen("fav:");
				if (pos != len) {
					std::string favname = inputText.substr(len, pos - len);
					std::string favval = inputText.substr(pos + 1);

					if (favval == "\"\"") {
						iniFile->write("Favorite", favname.c_str(), NULL);
						return 0;
					} else {
						iniFile->write("Favorite", favname.c_str(), favval.c_str());
						return 0;
					}
				}
			} else {
				char *favval = iniFile->read("Favorite", inputText.substr(strlen("fav:")).c_str());
				if (favval != NULL) {
					inputText = favval;
					free(favval);
				}

			}
		}
		if (inputText.compare("help:") == 0) {
			if (helpCmd == "") {
				char *temp;
				char *work = NULL;
				DWORD type;
	
				if ((temp = (char *)get_reg_value(hideinstloc, &type)) == NULL) {
					// �G�ۂ��C���X�g�[������Ă��Ȃ��Ȃ�A������
					helpCmd = "notepad.exe ";
					work = (char *)get_reg_value(turukamedir, &type);
					helpCmd += "\"";
					helpCmd += work;
					helpCmd += "\\HidemaruMailSearch.txt\"";
				} else {
					helpCmd = "\"";
					helpCmd += temp;
					helpCmd += "\\Hidemaru.exe\" \"";
					helpCmd += "/j";
					helpCmd += HELPFILE_LINE;
					helpCmd += " ";
					if ((work = (char *)get_reg_value(macropath, &type)) == NULL) {
						// �}�N���p�X�����݂��Ȃ���ΏG�ۂ� InstallLocation ���z�[��
						helpCmd += temp;
					} else {
						helpCmd += work;
					}
					helpCmd += "\\HidemaruMailSearch.txt\"";
				}
				if (temp != NULL) {
					free(temp);
				}
				if (work != NULL) {
					free(work);
				}
			}
			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			memset(&si, 0, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			CreateProcess(NULL, (LPSTR)helpCmd.c_str(), NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
			return 0;
		} else if (inputText.find("debug:") == 0) {
			query = inputText.substr(strlen("debug:"));
		} else {
			std::string flag;
			
			trim(inputText);
			while (inputText.size() != 0) {
				int tagKind = findTag(inputText);
				if (tagKind == TARGET_SMALLHEADERBODY && inputText.find(colon_targets[TARGET_SMALLHEADERBODY]) != 0) {
					inputText = colon_targets[TARGET_SMALLHEADERBODY] + inputText;
				}
				rc = makeQuery(tagKind, inputText, query, flag);
				trim(inputText);
			}
		
			if (flag.size() != 0) {
				if (query.size() != 0) {
					query += ",";
				}
				query += flag;
			}

			// �T�u�t�H���_�������Ώ̂ɂ��邩�ǂ���
			if (selectedRadioButton == IDC_RADIO2) {
				query += ",subfolder=1";
			} else {
				query += ",subfolder=0";
			}

			// ��Ƀn�C���C�g
			query += ",hilight=1";
		}

#ifdef _DEBUG
		println("���A�J�E���g");
		println(account);
		println("���t�H���_");
		println(folder);
		println("��������");
		println(query);
		println("������");
		println(history[0]);
#endif
		if (IsHidemaruMailMain() || IsHidemaruMailGrep()) {
			SetFindPack(query.c_str());
			StartDoGrep(account, folder);
			PushFindPack();
		} else {
			ExecAtMain("SetFindPack", query.c_str());
			ExecAtMain("StartDoGrep", account, folder);
			ExecAtMain("PushFindPack");
		}
	}
	return 0;
}

/**
 * DLL �̃��C���G���g���|�C���g�B
 * �ݒ�t�@�C��������e�̓ǂݏo�����s���Ă���B
 */
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	switch (ul_reason_for_call)	{
	case DLL_PROCESS_ATTACH: {

		// �ݒ�t�@�C��������e��ǂݏo��
		char *work = getMacroDir();
		std::string iniFilePath = work;
#ifdef _DEBUG
		std::string logFilePath = work;
		logFilePath += "\\HidemaruMailSearch.log";
		fopen_s(&logFile, logFilePath.c_str(), "w");
#endif
		free(work);
		iniFilePath += "\\HidemaruMailSearch.ini";

		iniFile = new IniFile(iniFilePath.c_str());

		for (int i = 0; i < MAX_HISTORY; i++) {
			std::string key = "history";
			char num[3] = {0};
			_itoa_s(i, num, 10);
			key += num;
			char *temp = iniFile->read("History", key.c_str());
			if (temp != NULL) {
				if (strlen(temp) != 0) {
					std::string value = temp;
					history.push_back(value);
				}
				free(temp);
			}
		}

		work = iniFile->read("Config", "radio");
		if (strcmp(work, "1") == 0) {
			selectedRadioButton = IDC_RADIO1;
		} else if (strcmp(work, "2") == 0) {
			selectedRadioButton = IDC_RADIO2;
		} else if (strcmp(work, "3") == 0) {
			selectedRadioButton = IDC_RADIO3;
		} else if (strcmp(work, "4") == 0) {
			selectedRadioButton = IDC_RADIO4;
		} else {
			selectedRadioButton = IDC_RADIO1;
		}
		if (work != NULL) {
			free(work);
		}

		work = iniFile->read("Config", "check");
		if (strcmp(work, "TRUE") == 0) {
			selectedCheckBox = TRUE;
		} else {
			selectedCheckBox = FALSE;
		}
		if (work != NULL) {
			free(work);
		}

		// TKInfo.dll �̏������ƃG�N�X�|�[�g�֐��̎擾
		char *hidemaruMailDir = (char *)get_reg_value(HIDEMARU_MAIL_DIR_KEY, NULL);
		std::string tkInfo = hidemaruMailDir;
		free(hidemaruMailDir);
		tkInfo += "TKInfo.dll";

		hInstDLL = LoadLibrary(tkInfo.c_str());

		GET_FUNC(HidemaruMailVersion);
		GET_FUNC(StartDoGrep);
		GET_FUNC(SetFindPack);
		GET_FUNC(CurrentAccount);
		GET_FUNC(CurrentFolder);
		GET_FUNC(YenEncode);
		GET_FUNC(ExecAtMain);
		GET_FUNC(SetQuietMode);
		GET_FUNC(IsHidemaruMail);
		GET_FUNC(IsHidemaruMailMain);
		GET_FUNC(IsHidemaruMailGrep);
		GET_FUNC(MainWnd);
		GET_FUNC(PushFindPack);

		break;
	}

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		if (iniFile != NULL) {
			delete iniFile;
		}
#ifdef _DEBUG
		fclose(logFile);
#endif
		break;
	}
    return TRUE;
}
