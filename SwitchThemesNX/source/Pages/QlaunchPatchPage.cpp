#include "QlaunchPatchPage.hpp"
#include "../ViewFunctions.hpp"
#include "RemoteInstall/Worker.hpp"

class ThemeUpdateDownloader : public RemoteInstall::Worker::BaseWorker {
public:
	struct Result {
		std::string error;
		long httpCode;
		std::vector<u8> data;
	};

	ThemeUpdateDownloader(const std::string& url, Result& r) : BaseWorker({url}, true), OutResult(r) {
		appendUrlToError = false;
		SetLoadingLine("正在检查补丁更新...");
	}

protected:
	void OnComplete() {
		const auto& str = Errors.str();
		if (str.length())
			OutResult.error = str;
		else
			OutResult.data = Results.at(0);
	}

	bool OnFinished(uintptr_t index, long httpCode) override {
		OutResult.httpCode = httpCode;
		return true;
	}

	Result& OutResult;
};

QlaunchPatchPage::QlaunchPatchPage() : IPage("主题补丁") { }

void QlaunchPatchPage::Render(int X, int Y)
{
	Utils::ImGuiSetupPage(this, X, Y);

	ImGui::TextWrapped(
		"从固件 9.0 开始，主菜单的某些部分需要修补才能安装主题。\n"
		"如果您看到这条提示，则表示您没有安装固件所需的补丁。"
	);	

	if (PatchMng::QlaunchBuildId() != "")
	{
		ImGui::Text("您的主菜单版本如下 （BuildID）:");
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::Highlight);
		Utils::ImGuiCenterString(PatchMng::QlaunchBuildId());
		ImGui::PopStyleColor();
	}
	else 
	{
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::Red);
		ImGui::Text("错误：无法检测到您的主菜单版本");
		ImGui::PopStyleColor();
	}

	if (patchStatus == PatchMng::InstallResult::MissingIps) 
	{		
		ImGui::TextWrapped("当前不支持此版本，发布新固件后，可能需要几天时间才能更新补丁");
		ImGui::TextWrapped(
			"现在，每当您启动此应用程序时，都会自动从互联网下载新的补丁。"
			"如果需要，您还可以立即检查更新。"
		);
		
		if (ImGui::Button("检查更新"))
			PushFunction([this]() { CheckForUpdates(); });

		if (updateMessageString != "")
		{
			ImGui::SameLine();

			if (updateMessageIsError)
				ImGui::PushStyleColor(ImGuiCol_Text, Colors::Red);
			else ImGui::PushStyleColor(ImGuiCol_Text, Colors::Highlight);
			
			ImGui::TextWrapped(updateMessageString.c_str());
			ImGui::PopStyleColor();
		}

		ImGui::TextWrapped(
			"如果您不想将主机连接到互联网，请点击以下链接:"
		);
		
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::Highlight);
		ImGui::Text("https://github.com/exelix11/theme-patches");
		ImGui::PopStyleColor();
	}
	else if (patchStatus == PatchMng::InstallResult::SDError)
	{
		ImGui::TextWrapped(
			"从 SD 卡读取或写入文件时出错，这通常意味着您的 SD 已损坏。\n"
			"请运行存档位修复器，如果仍然不起作用，请格式化您的 SD 并从头开始设置。"
		);
	}
	else if (patchStatus == PatchMng::InstallResult::UnsupportedCFW)
	{
		ImGui::TextWrapped(
			"你的自制固件似乎不受支持。\n"
			"如果你的自制固件受支持，但你看到这个提示可能是你的 SD 卡有问题，请重新安装你的自制固件。"
		);
	}
	else if (patchStatus == PatchMng::InstallResult::Ok)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, Colors::Highlight);
		ImGui::Text("已成功更新，请重新启动您的主机！");
		ImGui::PopStyleColor();
	}

	Utils::ImGuiSetWindowScrollable();
	Utils::ImGuiCloseWin();
}

void QlaunchPatchPage::Update()
{
	if (Utils::PageLeaveFocusInput())
		Parent->PageLeaveFocus(this);
}

void QlaunchPatchPage::CheckForUpdates() {
	ThemeUpdateDownloader::Result res;
	PushPageBlocking(new ThemeUpdateDownloader("https://exelix11.github.io/theme-patches/ips/" + PatchMng::QlaunchBuildId(), res));

	if (res.error != "")
	{
		updateMessageIsError = true;
		updateMessageString = res.error;
	}
	else if (res.httpCode == 404)
	{
		updateMessageIsError = false;
		updateMessageString = "未找到更新";
	}
	else if (res.httpCode != 200)
	{
		updateMessageIsError = true;
		updateMessageString = "HTTP错误: 代码 " + res.httpCode;
	}
	else
	{
		updateMessageIsError = false;
		fs::patches::WritePatchForBuild(PatchMng::QlaunchBuildId(), res.data);
		patchStatus = PatchMng::EnsureInstalled();
		updateMessageString = "已成功更新，请重新启动您的主机！";
	}
}

bool QlaunchPatchPage::ShouldShow()
{
	patchStatus = PatchMng::EnsureInstalled();

	if (patchStatus == PatchMng::InstallResult::Ok)
		return false;

	if (patchStatus == PatchMng::InstallResult::MissingIps)
	{
		CheckForUpdates();
		// Has anything changed ? 
		if (patchStatus == PatchMng::InstallResult::Ok)
			return false;
	}
	
	return true;
}
