#define EXPORT extern "C" __declspec(dllexport) 

#pragma warning(disable:4312) // 32bit から 64bit へのキャスト時の警告を無視
#pragma warning(disable:4100) // 一度も使用しない引数の警告を無視
#pragma warning(disable:4706) // 条件式内部での値の代入の警告を無視

// 各種定義
#define HIDEMARU_MAIL_DIR_KEY "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\TuruKameDir"
#define MAX_HISTORY (20)
#define HELPFILE_LINE "86" // HidemaruMailSearch.txtのコマンド説明開始行の行番号

#define WIN32_LEAN_AND_MEAN		// Windows ヘッダーから使用されていない部分を除外します。
#include <windows.h>
#include <windowsx.h>
#include <imm.h> // IME の状態変更
#include <string>
#include <vector>
#include "resource.h"
#include "registory.h"
#include "IniFile.h"
#include "boost/regex.hpp"
#include "boost/tokenizer.hpp"

// 関数マクロ
#define DEF_FUNC(fn) fn##_ fn;
#define GET_FUNC(fn) fn = (fn##_)GetProcAddress(hInstDLL, #fn);


// TKInfo.dll の関数定義
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
 * この DLL そのもののインスタンス。
 */
static HINSTANCE hInst = NULL;

/**
 * TkInfo.dll のインスタンス。
 */
static HINSTANCE hInstDLL = NULL;

/**
 * 設定ファイルを操作するインスタンスを保持する変数。
 *
 * DLL の ATTACH で初期化され、DLL の DETACH で開放される。
 */
static IniFile *iniFile = NULL;

/**
 * 検索文字列の履歴を格納する配列。
 * 
 * DLL の ATTACH で INI ファイルから読み出され初期化される他、
 * OK ボタン押下時に更新される。
 * 格納時に重複が前方優先で削除される。
 *
 * 履歴のサイズは MAX_HISTORY までとする。
 */
static std::vector<std::string> history;

/**
 * 選択されているラジオボタンの ID を格納する変数。
 *
 * DLL の ATTACH で INI ファイルから読み出され初期化される他、
 * OK ボタン押下時に更新される。
 */
static int selectedRadioButton;

/**
 * チェックボックスの選択状態を格納する変数。
 *
 * DLL の ATTACH で INI ファイルから読み出され初期化される他、
 * OK ボタン押下時に更新される。
 *
 * @li TRUE チェックされている
 * @li FALSE チェックされていない
 */
static int selectedCheckBox;

/**
 * マクロ起動時にユーザが選択している文字列を格納する変数。
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
 * ヘルプを表示するコマンドライン。
 */
static std::string helpCmd("");

#define	macropath "HKEY_CURRENT_USER\\Software\\Hidemaruo\\Hidemaru\\Env\\MacroPath"
#define	turukamedir "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\TuruKameDir"
#define	hideinstloc "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Hidemaru\\InstallLocation"

/**
 * INI ファイルを保存するディレクトリ（このモジュールが配置されているディレクトリ）を取得する。
 *
 * この関数は成功するとディレクトリをポインタで返す。
 * 呼び出し元は戻り値が NULL ではないなら、ポインタを適切に開放しなければならない。
 *
 * @return ディレクトリパス
 */
static char *getMacroDir(void) {
	char *work;
	char *temp;
	DWORD type;
	
	if ((temp = (char *)get_reg_value(hideinstloc, &type)) == NULL) {
		// 秀丸がインストールされていないなら、秀丸メールディレクトリがホーム
		work = (char *)get_reg_value(turukamedir, &type);
	} else {
		// 秀丸がインストールされているならマクロパスを調べる
		if ((work = (char *)get_reg_value(macropath, &type)) == NULL) {
			// マクロパスが存在しなければ秀丸の InstallLocation がホーム
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
 * ダイアログを画面の中央に配置する。
 * @param [in] hwnd 中央に配置するダイアログのハンドル
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
 * ダイアログのプロシージャ。
 * @param [in] hDlg ダイアログボックスのハンドルが格納されています
 * @param [in] uMsg メッセージが格納されています
 * @param [in] wParam メッセージの追加情報が格納されています
 * @param [in] lParam メッセージの追加情報が格納されています
 * @return メッセージを処理した場合は 0 以外(TRUE)、そうでなければ 0(FALSE)
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

		// SetFocus でフォーカス移動を行った場合は FALSE を返さなければならない
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

			// 設定ファイルに状態を書き出す
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

			// 履歴を保存
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
			case CBN_SELCHANGE: // 選択項目が変更されたとき
			case CBN_EDITUPDATE: // 直接入力で内容が変更されたとき。変更内容が表示される前に呼ばれる
			case CBN_EDITCHANGE: // 直接入力で内容が変更されたとき。変更内容が表示された後に呼ばれる
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
 * 入力文字の前後のスペースを削除する。
 * @param [in,out] inputText 対象の文字列
 */
static void trim(std::string &inputText) {
	boost::smatch m;
	boost::regex ftrim;
	boost::regex rtrim;
	// 全角スペースをセパレータとして扱うオプション
	if (!strcmp("1", iniFile->read("Option", "fullwidthspace_separator"))) {
		ftrim = boost::regex("^[ 　]+");
		rtrim = boost::regex("[ 　]+$");
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
 * 見つかったクエリーの種類を返す。
 * @param [in] inputText 検索対象
 * @return 見つかったクエリの種類
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
 * 秀丸メールの仕様にあわせた検索文字列を作成する。
 *
 * @param [in] tagIndex 検索対象（"body"、"header" など）のインデックス
 * @param [in,out] inputText ユーザの入力文字列
 * @param [in,out] query 作成した検索文字列
 * @param [in,out] flag 作成したフラグ文字列
 * @return 動作結果
 */
static int makeQuery(int tagIndex, std::string &inputText, std::string &query, std::string &flag) {
	// 添付ファイル有無を最初に調査する
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
	// フラグチェック
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

	// 正規表現フラグチェック
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
			// 全角スペースをセパレータとして扱うオプション
			if (!strcmp("1", iniFile->read("Option", "fullwidthspace_separator"))) {
				size_t i = 0;
				if ((i = s.find("　", i)) != std::string::npos) {
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
		//inputText = boost::regex_replace(inputText, r, "", boost::format_all);  // ← これでも同じ、って言いたかったコメントアウトかなぁ？
		inputText.erase(inputText.find(deleteTarget), deleteTarget.size());
	} else {
		inputText = colon_targets[TARGET_SMALLHEADERBODY] + inputText;
	}
	return rc;
}

/**
 * マクロから呼び出されるエントリポイント。
 * @param [in] text 検索文字列
 * @return 戻り値に意味はない
 */
EXPORT int search(const char *text) {
	if (!IsHidemaruMail()) {
#ifndef _DEBUG
		MessageBox(NULL, "このマクロは秀丸からは起動できません。", NULL, MB_OK);
		return 0;
#endif
	}
	HWND hwnd = reinterpret_cast<HWND>(MainWnd());

	if (hwnd == NULL) {
		MessageBox(NULL, "秀丸メール本体が起動してなければなりません。", NULL, MB_OK);
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

// SetQuietMode を使用すると、検索結果ウィンドウが手前に来ない
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
				folder = "受信+送信済み+ユーザー";
			}
			break;

		case IDC_RADIO4:
			account = "";
			if (selectedCheckBox) {
				folder = "";
			} else {
				folder = "受信+送信済み+ユーザー";
			}
			break;
		}

#ifdef _DEBUG
switch (selectedRadioButton) {
case IDC_RADIO1:
case IDC_RADIO2:
case IDC_RADIO3:
	if (account == NULL || strlen(account) == 0) {
		MessageBox(NULL, "秀丸メールにアカウントが見つかりませんでした。\nこのエラーが頻発するようなら作者に連絡してください。", "HidemaruMailSearch", MB_OK);
	}
	break;
}
#endif

		std::string inputText = history[0].c_str();
		std::string query;
		trim(inputText);

		// お気に入りの保存と読み込み
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
					// 秀丸がインストールされていないなら、メモ帳
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
						// マクロパスが存在しなければ秀丸の InstallLocation がホーム
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

			// サブフォルダを検索対称にするかどうか
			if (selectedRadioButton == IDC_RADIO2) {
				query += ",subfolder=1";
			} else {
				query += ",subfolder=0";
			}

			// 常にハイライト
			query += ",hilight=1";
		}

#ifdef _DEBUG
		println("■アカウント");
		println(account);
		println("■フォルダ");
		println(folder);
		println("■検索式");
		println(query);
		println("■入力");
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
 * DLL のメインエントリポイント。
 * 設定ファイルから内容の読み出しを行っている。
 */
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	switch (ul_reason_for_call)	{
	case DLL_PROCESS_ATTACH: {

		// 設定ファイルから内容を読み出す
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

		// TKInfo.dll の初期化とエクスポート関数の取得
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
