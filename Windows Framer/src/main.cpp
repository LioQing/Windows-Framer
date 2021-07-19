#include <cstdio>
#include <iostream>
#include <list>
#include <SFML/Graphics.hpp>
#include <windows.h>
#include <imgui.h>
#include <imgui-SFML.h>
#include <atomic>
#include <thread>

#include "resource.h"

#define WM_TRAY (WM_USER + 100)
#define WM_TASKBAR_CREATED RegisterWindowMessage(TEXT("TaskbarCreated"))

#define APP_NAME TEXT("Windows Framer")
#define APP_TIP TEXT("Windows Framer hiding here")

NOTIFYICONDATA nid; //Tray attribute
HMENU hMenu; //Tray menu

std::atomic_bool isHidden = false;
std::atomic_bool isClosed = false;

 // instantiate the tray
void InitTray(HINSTANCE hInstance, HWND hWnd)
{
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAY));
    lstrcpy(nid.szTip, APP_NAME);

    hMenu = CreatePopupMenu();//Generate tray menu
    // Add two options for the tray menu
    AppendMenu(hMenu, MF_STRING, ID_SHOW, TEXT("windows framer editor"));
    AppendMenu(hMenu, MF_STRING, ID_EXIT, TEXT("exit"));

    Shell_NotifyIcon(NIM_ADD, &nid);
}

//Presentation tray bubble reminder
void ShowTrayMsg()
{
    lstrcpy(nid.szInfoTitle, APP_NAME);
    lstrcpy(nid.szInfo, TEXT("receive a message!"));
    nid.uTimeout = 1000;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (isClosed)
    {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
    }

    switch (uMsg)
    {
    case WM_TRAY:
        switch (lParam)
        {
        case WM_RBUTTONDOWN:
        {
            // Get mouse coordinates
                POINT pt; GetCursorPos(&pt);

            // Solve the problem that clicking the left button menu outside the menu does not disappear
                SetForegroundWindow(hWnd);

            // Make the menu grayed out
                //EnableMenuItem(hMenu, ID_SHOW, MF_GRAYED);	

                // Display and get the selected menu
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, NULL, hWnd,
                    NULL);
                if (cmd == ID_SHOW)
                    isHidden = false;
            if (cmd == ID_EXIT)
                PostMessage(hWnd, WM_DESTROY, NULL, NULL);
        }
        break;
        case WM_LBUTTONDOWN:
            isHidden = false;
            break;
        case WM_LBUTTONDBLCLK:
            break;
        }
        break;
    case WM_DESTROY:
        // Delete the tray when the window is destroyed
            Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        isClosed = true;
        break;
    case WM_TIMER:
        ShowTrayMsg();
        KillTimer(hWnd, wParam);
        break;
    }
    if (uMsg == WM_TASKBAR_CREATED)
    {
        // System Explorer crash restart, reload the tray
            Shell_NotifyIcon(NIM_ADD, &nid);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


class MouseManager
{
public:

    std::vector<sf::Vector2i> pos;
    std::vector<sf::Vector2i> size;
    sf::Vector2i* currentPos = nullptr;
    sf::Vector2i* currentSize = nullptr;
    bool isDown = false;

    void OnMousePressed(sf::Mouse::Button button, unsigned int x, unsigned int y, std::vector<sf::RectangleShape>& shapes, ImVec2 impos, ImVec2 imsize)
    {
        if (x > impos.x && x < impos.x + imsize.x &&
            y > impos.y && y < impos.y + imsize.y)
            return;

        if (button == sf::Mouse::Button::Left)
        {
            sf::RectangleShape newRect;
            newRect.setFillColor(sf::Color::White);
            newRect.setOutlineThickness(1);
            newRect.setOutlineColor(sf::Color::Black);
            shapes.push_back(newRect);

            isDown = true;

            pos.push_back(sf::Vector2i(x, y));
            size.push_back(sf::Vector2i(0, 0));

            currentPos = &pos[pos.size() - 1];
            currentSize = &size[size.size() - 1];
        }
        else if (button == sf::Mouse::Button::Right)
        {
            if (isDown)
                OnMouseReleased(sf::Mouse::Button::Left, x, y, shapes);

            for (auto i = 0; i < pos.size(); ++i)
            {
                if (x > pos[i].x && x < pos[i].x + size[i].x &&
                    y > pos[i].y && y < pos[i].y + size[i].y)
                {
                    pos.erase(pos.begin() + i);
                    size.erase(size.begin() + i);
                    shapes.erase(shapes.begin() + i);
                    return;
                }
            }
        }
    }

    void OnMouseReleased(sf::Mouse::Button button, unsigned int x, unsigned int y, std::vector<sf::RectangleShape>& shapes)
    {
        if (button == sf::Mouse::Button::Left)
        {
            if (!isDown) return;

            isDown = false;

            auto tmp = *currentPos;
            currentPos->x = std::min(currentPos->x, (int)x);
            currentPos->y = std::min(currentPos->y, (int)y);

            currentSize->x = std::max((int)x - tmp.x, tmp.x - (int)x);
            currentSize->y = std::max((int)y - tmp.y, tmp.y - (int)y);

            for (auto i = 0; i < pos.size(); ++i)
            {
                shapes[i].setPosition(pos[i].x, pos[i].y);
                shapes[i].setSize({ (float)size[i].x, (float)size[i].y });
            }

            currentPos = nullptr;
            currentSize = nullptr;
        }
    }
};

void EditorWindow(MouseManager& mm, sf::RenderWindow& window, HWND hWnd, std::vector<sf::RectangleShape>& shapes)
{
    ImVec2 impos, imsize;

    sf::Clock deltaClock;
    while (window.isOpen())
    {
        // event 

        sf::Event event;
        while (window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);

            switch (event.type)
            {
            case sf::Event::Closed:
            {
                window.close();
                isClosed = true;
                break;
            }
            case sf::Event::MouseButtonPressed:
            {
                mm.OnMousePressed(event.mouseButton.button, event.mouseButton.x, event.mouseButton.y, shapes, impos, imsize);
                break;
            }
            case sf::Event::MouseButtonReleased:
            {
                mm.OnMouseReleased(event.mouseButton.button, event.mouseButton.x, event.mouseButton.y, shapes);
                break;
            }
            }
        }

        //update

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::Begin("Utilities");
        if (ImGui::Button("Click here to minimize"))
        {
            isHidden = true;
        }

        impos = ImGui::GetWindowPos();
        imsize = ImGui::GetWindowSize();

        ImGui::End();

        if (mm.isDown)
        {
            auto i = mm.pos.size() - 1;
            shapes[i].setPosition(mm.pos[i].x, mm.pos[i].y);
            shapes[i].setSize({ (float)sf::Mouse::getPosition().x - mm.pos[i].x, (float)sf::Mouse::getPosition().y - mm.pos[i].y });
        }

        if (isClosed)
            window.close();

        if (isHidden)
            window.setVisible(false);
        else
            window.setVisible(true);


        // draw

        window.clear();

        SetLayeredWindowAttributes(hWnd, 0, 128, LWA_ALPHA);
        for (auto& s : shapes) window.draw(s);

        ImGui::SFML::Render(window);

        window.display();
    }
}

void TrayHandling()
{
    HINSTANCE hInstance = NULL;
    HWND thWnd;
    MSG msg;
    WNDCLASS wc = { 0 };
    wc.style = NULL;
    wc.hIcon = NULL;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.lpfnWndProc = WndProc;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = APP_NAME;
    wc.hCursor = NULL;

    if (!RegisterClass(&wc)) return;

    thWnd = CreateWindowEx(WS_EX_TOOLWINDOW, APP_NAME, APP_NAME, WS_POPUP, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    ShowWindow(thWnd, SW_HIDE);
    UpdateWindow(thWnd);

    InitTray(hInstance, thWnd); // instantiate the tray
    SetTimer(thWnd, 3, 1000, NULL); //Timed message, demo bubble prompt function

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int main()
{
    // setter window
    MouseManager mm;

    auto screen = sf::VideoMode::getDesktopMode();
    ++screen.width; ++screen.height;

    sf::RenderWindow window(screen, "", sf::Style::None);

    ImGui::SFML::Init(window);

    HWND hWnd = window.getSystemHandle();
    SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    
    std::vector<sf::RectangleShape> shapes;

    // tray handling window
    std::thread t(TrayHandling);

    EditorWindow(mm, window, hWnd, shapes);

    ImGui::SFML::Shutdown();
    t.join();

    return 0;
}