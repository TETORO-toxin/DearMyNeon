#include <windows.h>
#include "DxLib.h"
#include "game_manager.h" // ゲームマネージャーをインクルード
#include <exception>
#include "Code/debug_utils.h"
#include <EffekseerForDXLib.h>

//-------------------------------------------------------//
//　　　　　　　　【なつやすみかだい】　　　　　　　　 　　  //
// 自分の好きなゲームを作ってみよう！ 　　　　　　　　       //
//                                                        //
// Init(),Update()に処理を実装しよう！　　　　　　　　  　  //
// 必要な変数,関数はmain.hに追加してみよう！　　　　        //
// 使いたい処理があれば下記リンクから探して使ってみよう！    //
// https://dxlib.xsrv.jp/dxfunc.html                    //
// 　　　　　　　　　　　　　　　　　　　　　　　　　　　     //
//-------------------------------------------------------//
namespace
{
    const static int WindowW = 1920;
    const static int WindowH = 1080;
}

// --- WinMain関数: プログラムのエントリーポイント ---
// この関数はDxLibを使用するWindowsアプリケーションのエントリーポイントです
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

    
        
    // Make the process DPI aware so monitor coordinates and window sizing are not scaled by Windows.
    // This helps borderless-fullscreen positioning on Windows 10/11.
    SetProcessDPIAware();

    // DxLibの初期設定
    // DxLibのログファイル(Log.txt)を生成しないように設定
    SetOutApplicationLogValidFlag(FALSE);
    // ウィンドウモードで実行するように設定 (TRUEでウィンドウモード、FALSEでフルスクリーン)
    // Start in fullscreen
    ChangeWindowMode(FALSE);
    // 画面の解像度と色深度を設定 (define.hで定義した定数を使用)
    SetGraphMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    // ウィンドウのタイトルバーに表示されるテキストを設定
    SetMainWindowText("Dear:MYNEON");

    // DxLibの初期化
    // DxLibを使用するための初期設定を行います。失敗すると-1を返します。
    if (DxLib_Init() == -1) {
        MessageBox(NULL, "Failed to initialize DxLib.", "Error", MB_OK);
        return -1; // 初期化失敗時はプログラムを終了
    }

    // 描画先を裏画面に設定 (ダブルバッファリング)
    // これにより、画面のちらつきを防ぎ、スムーズな描画が可能になります。
    SetDrawScreen(DX_SCREEN_BACK);
   
    try {
        // GameManagerのインスタンスを取得し、全リソースを読み込む
        // GameManagerはシングルトンパターンなので、GetInstance()で唯一のインスタンスを取得します。
        // 初回呼び出し時にインスタンスが生成され、必要な画像やサウンドがロードされます。
        // Start asynchronous loading; a loading screen will be shown until completion
        GameManager::GetInstance().StartAsyncLoad();

        // --- ゲームのメインループ ---
        // ProcessMessage() == 0: ウィンドウメッセージの処理（ウィンドウの終了ボタンクリックなど）
        // CheckHitKey(KEY_INPUT_ESCAPE) == 0: ESCキーが押されていないかを確認（ESCキーでゲーム終了）
        // Fixed timestep configuration
        const int TARGET_FPS = 60;
        const int TARGET_FRAME_MS = 1000 / TARGET_FPS;
        int lastFrameTime = GetNowCount();

#if DEBUG_ACTIVE
        while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0)
#else
        while (ProcessMessage() == 0)
#endif
        {
            // 描画画面をクリア
            // 毎フレーム描画内容を消去し、新しいフレームの描画に備えます。
            ClsDrawScreen();

            // GameManagerにゲーム全体の更新と描画を委ねる
            // 現在のゲームの状態（タイトル、ゲームプレイ中など）に応じて、
            // 適切な更新・描画処理が内部で実行されます。
            GameManager::GetInstance().UpdateGame();
            GameManager::GetInstance().DrawGame();

            // 裏画面の内容を表画面に反映
            // ここまで裏画面に描画した内容が、実際にユーザーに見える画面に表示されます。
            ScreenFlip();

            // Frame limiting to achieve approximately TARGET_FPS
            int now = GetNowCount();
            int elapsed = now - lastFrameTime;
            if (elapsed < TARGET_FRAME_MS) {
                WaitTimer(TARGET_FRAME_MS - elapsed);
                // update now after waiting to keep cadence
                now = GetNowCount();
            }
            lastFrameTime = now;
        }
    } catch (const std::exception& ex) {
        char buf[1024];
        sprintf_s(buf, "Unhandled exception: %s", ex.what());
        MessageBox(NULL, buf, "Exception", MB_OK | MB_ICONERROR);
    } catch (...) {
        MessageBox(NULL, "Unhandled unknown exception", "Exception", MB_OK | MB_ICONERROR);
    }

    // --- 終了処理 ---
    // GameManagerのシングルトンインスタンスを解放
    // newで確保されたメモリはdeleteで解放する必要があります。
    // これによりGameManagerのデストラクタが呼ばれ、ロードしたリソースが適切に解放されます。
    // GetInstance()が返す参照のポインタをdeleteします
    GameManager::DestroyInstance();
    // DxLibの終了処理
    // DxLibが使用したリソースを解放します。
    DxLib_End();
    return 0; // プログラム正常終了
}

