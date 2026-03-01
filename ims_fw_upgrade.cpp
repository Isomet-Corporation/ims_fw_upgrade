// needed for PI
#define _USE_MATH_DEFINES

#include "ConnectionList.h"
#include "IEventHandler.h"
#include "IMSSystem.h"
#include "FirmwareUpgrade.h"
#include "cxxopts.h"
#include "LibVersion.h"

#include "ProgressBar.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <conio.h>
#include <atomic>

using namespace iMS;

class FirmwareUpgradeSupervisor : public IEventHandler
{
public:
	void EventAction(void* sender, const int message, const int param)
	{
		switch (message)
		{
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_DONE): std::cout << "F/W Upgrade: DONE!" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERROR): std::cout << "F/W Upgrade: >> ERROR <<" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_STARTED): std::cout << "F/W Upgrade: Started" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_INITIALIZE_OK): std::cout << "F/W Upgrade: Initialize OK" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_CHECKID_OK): std::cout << "F/W Upgrade: Check ID OK" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ENTER_UG_MODE): std::cout << "F/W Upgrade: Entered Upgrade Mode. Erasing..." << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERASE_OK): std::cout << "F/W Upgrade: Erase OK" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_PROGRAM_OK): //std::cout << "F/W Upgrade: Program OK" << std::endl;
			//std::cout << "Please cycle power to enable upgrade" << std::endl;
			break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_VERIFY_OK): std::cout << "F/W Upgrade: Verify OK" << std::endl; break;
		case (FirmwareUpgradeEvents::FIRMWARE_UPGRADE_LEAVE_UG_MODE): std::cout << "F/W Upgrade: Leaving Upgrade Mode" << std::endl; break;
		}
	}
};

bool query = false;
bool verify = false;
bool prog = true;
std::string mcs;

//cxxopts::ParseResult
void
parse(int argc, char* argv[])
{
	cxxopts::Options options("ims_fw_upgrade", " - iMS Firmware Upgrade Utility");
	try
	{
		options
			.add_options()
			("q,query", "Query current iMS firmware details then exit", cxxopts::value<bool>(query))
			("v,verify", "Verify Integrity Only of existing Firmware in iMS non-volatile memory", cxxopts::value<bool>(verify))
			("h,help", "Print help")
			("m,mcs", "MCS File to download", cxxopts::value<std::string>(mcs))
			;
		options.positional_help("<Filename of Firmware Upgrade .MCS>");
		options.parse_positional({ "mcs" });  // Last argument must be MCS file

		auto result = options.parse(argc, argv);
		if (result.count("help") || result.count("h") || !result.arguments().size())
		{
			std::cout << options.help() << std::endl;
			prog = false;
			return;
		}

	}
	catch (const cxxopts::OptionException& e)
	{
		std::cout << "error parsing options: " << e.what() << std::endl;
		std::cout << options.help() << std::endl;
		prog = false;
		return;
	}
}

int main(int argc, char** argv)
{
	std::cout << std::endl << "iMS Firmware Upgrade Utility v2.0.1" << std::endl;
	std::cout << "(c) Isomet (UK) Ltd 2018-26" << std::endl;
	std::cout << "Built on iMS SDK v" << LibVersion::GetVersion() << std::endl;
	std::cout << "==================================================================" << std::endl;

	parse(argc, argv);
	if (!prog) {
		std::cout << std::endl << "Press any key to exit ... " << std::endl;
		while (!_kbhit());
		return 0;
	}

	if (prog && !query && !verify) {
		std::ifstream f(mcs.c_str());
		if (!f.good()) {
			std::cout << std::endl << "Unable to open " << mcs << ". Please check and try again." << std::endl;
			std::cout << std::endl << "Press any key to exit ... " << std::endl;
			while (!_kbhit());
			return -1;
		}
		else {
			std::cout << std::endl << "Upgrading from " << mcs << std::endl;
		}
	}

	std::cout << std::endl << "Scanning for iMS Systems ... please wait ... " << std::endl;
	std::unique_ptr<ConnectionList> connList(new ConnectionList());
	auto& modules = connList->modules();
	if (std::find(modules.begin(), modules.end(), "CM_SERIAL") != modules.end()) connList->config("CM_SERIAL").IncludeInScan = false;
	auto fulliMSList = connList->Scan();

	if (fulliMSList.size() > 0) {
		std::cout << "Discovered " << fulliMSList.size() << " iMS Systems: " << std::endl;
		int ims_count = 1;
		for (auto ims: fulliMSList) {
			std::cout << "  " << ims_count++ << ") " << ims->ConnPort() << std::endl;
		}
		std::shared_ptr<IMSSystem> myiMS;
		for (;;) {
			std::cout << "Select a system to continue ... ";
			char ch;
			std::cin >> ch;
			if ((ch - '0') > 0 && (ch - '0') <= (int)(fulliMSList.size())) {
				myiMS = fulliMSList[ch - '1'];
				break;
			}
		}

		auto syn_model = myiMS->Synth().Model();
		auto ctlr_model = myiMS->Ctlr().Model();

		myiMS->Connect();
		myiMS->SetTimeouts(100, 100, 500, 500);
		std::cout << std::endl << "Connecting to IMS System on port: " << myiMS->ConnPort() << std::endl;
		if (myiMS->Synth().IsValid()) {
			std::cout << "Found Synthesiser: " << myiMS->Synth().Model() << " : " << myiMS->Synth().Description() << std::endl;
			std::cout << " FW Version: " << myiMS->Synth().GetVersion() << std::endl;
			std::cout << " Remote Upgrade supported: " << std::boolalpha << myiMS->Synth().GetCap().RemoteUpgrade << std::endl;
		}
		if (myiMS->Ctlr().IsValid() && (syn_model != ctlr_model)) {
			std::cout << "Found Controller: " << myiMS->Ctlr().Model() << " : " << myiMS->Ctlr().Description() << std::endl;
			std::cout << " FW Version: " << myiMS->Ctlr().GetVersion() << std::endl;
			std::cout << " Remote Upgrade supported: " << std::boolalpha << myiMS->Ctlr().GetCap().RemoteUpgrade << std::endl;
		}
		std::cout << std::endl;

		if (!query) {
			FirmwareUpgrade::UpgradeTarget tgt = FirmwareUpgrade::UpgradeTarget::SYNTHESISER;
			if ( (myiMS->Synth().GetCap().RemoteUpgrade && !myiMS->Ctlr().GetCap().RemoteUpgrade) || 
				 (syn_model == ctlr_model) ) {
				std::cout << "Targetting Synthesiser..." << std::endl;
			}
			else if (myiMS->Ctlr().GetCap().RemoteUpgrade && !myiMS->Synth().GetCap().RemoteUpgrade) {
				std::cout << "Targetting Controller..." << std::endl;
				tgt = FirmwareUpgrade::UpgradeTarget::CONTROLLER;
			}
			else if (myiMS->Ctlr().GetCap().RemoteUpgrade && myiMS->Synth().GetCap().RemoteUpgrade) {
				while (1) {
					std::cout << "Select Target: (s)ynthesiser or (c)ontroller ? ";
					char ch;
					std::cin >> ch;
					if (ch == 's' || ch == 'S') {
						break;
					}
					else if (ch == 'c' || ch == 'C') {
						tgt = FirmwareUpgrade::UpgradeTarget::CONTROLLER;
						break;
					}
				}
			}
			else {
				std::cout << "No supported upgrade targets. Exiting.." << std::endl;
			}
			std::cout << std::endl;

			std::ifstream ug(mcs.c_str(), std::ios::in);
			FirmwareUpgrade fwu(myiMS, ug);
			FirmwareUpgradeSupervisor fwus;

			auto progress = fwu.GetUpgradeProgress();
			if (progress.Started()) {
				std::cout << "ERROR: F/W upgrader not idle.  Please reset device and try again" << std::endl;
			}

			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_DONE, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERROR, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_STARTED, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_INITIALIZE_OK, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_CHECKID_OK, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ENTER_UG_MODE, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERASE_OK, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_PROGRAM_OK, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_VERIFY_OK, &fwus);
			fwu.FirmwareUpgradeEventSubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_LEAVE_UG_MODE, &fwus);

			ProgressBar bar(
				[&] { return fwu.GetTransferLength(); },
				[&] { return fwu.GetTotalTransferLength(); }
			);

			if (verify) {
				fwu.VerifyIntegrity(tgt);
			}
			else {
				fwu.StartUpgrade(tgt);
			}

			while (!fwu.UpgradeDone()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				auto prog = fwu.GetUpgradeProgress();
				if (fwu.UpgradeError()) {
					bar.stop();
					auto err = fwu.GetUpgradeError();
					if (err.StreamError()) {
						std::cout << "F/W Upgrade: Stream Error" << std::endl;
						std::cout << "   - Check MCS File Access permissions" << std::endl;
					}
					if (err.IdCode()) {
						std::cout << "F/W Upgrade: Unrecognised Flash ID Code" << std::endl;
						std::cout << "   - Please contact Isomet" << std::endl; 
					}
					if (err.Erase()) {
						std::cout << "F/W Upgrade: Erase Error" << std::endl;
						std::cout << "   - Please contact Isomet" << std::endl;
					}
					if (err.Program()) {
						std::cout << "F/W Upgrade: Program Error" << std::endl;
						std::cout << "   - Please contact Isomet" << std::endl;
					}
					if (err.TimeOut()) {
						std::cout << "F/W Upgrade: Timed Out" << std::endl;
						std::cout << "   - Please try upgrade again" << std::endl;
					}
					if (err.Crc()) {
						std::cout << "F/W Upgrade: CRC Error" << std::endl;
						std::cout << "   - Please check file contents are valid MCS format" << std::endl;
					}
					while (!_kbhit());
					return -1;
				}
				if (prog.EraseOK() && !prog.ProgramOK() && !bar.is_running()) {
					bar.start();
				}
				if (prog.ProgramOK() && bar.is_running()) {
					bar.stop();
					std::cout << "F/W Upgrade: Program OK" << std::endl;
					std::cout << "Please cycle power to enable upgrade" << std::endl;
				}
			}

			bar.stop();

			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_DONE, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERROR, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_STARTED, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_INITIALIZE_OK, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_CHECKID_OK, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ENTER_UG_MODE, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERASE_OK, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_PROGRAM_OK, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_VERIFY_OK, &fwus);
			fwu.FirmwareUpgradeEventUnsubscribe(FirmwareUpgradeEvents::FIRMWARE_UPGRADE_LEAVE_UG_MODE, &fwus);

		}
		std::cout << std::endl << "Press any key to exit ... ";
		while (!_kbhit());
		return 0;
	}
	else {
		std::cout << std::endl << "Cannot find system to upgrade. Please check connection and try again." << std::endl;
		std::cout << std::endl << "Press any key to exit ... ";
		while (!_kbhit());
		return (-1);

	}
}
