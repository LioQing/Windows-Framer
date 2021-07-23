#include <cstdio>
#include <iostream>
#include <list>
#include <SFML/Graphics.hpp>
#include <windows.h>
#include <imgui.h>
#include <imgui-SFML.h>
#include <atomic>
#include <thread>
#include <array>
#include <dwmapi.h>

#include <resource.h>

#define WM_TRAY (WM_USER + 100)
#define WM_TASKBAR_CREATED RegisterWindowMessage(TEXT("TaskbarCreated"))

#define APP_NAME TEXT("Windows Framer")

// todo
// edit rects
// save configs

NOTIFYICONDATA nid; //Tray attribute
HMENU hMenu; //Tray menu

const sf::Color rectCol(100, 100, 100, 128);

std::atomic_bool isHidden = false;
std::atomic_bool isClosed = false;

sf::Vector2i screenSize;
sf::Vector2i midPt;

int round(int n, int m)
{
    float a = (float)n / m;
    int b = std::ceil(a) * m;
    int c = std::floor(a) * m;

    return (std::abs(n - b) <= std::abs(n - c)) ? b : c;
}

RECT GetTitleBarRect(HWND hWnd)
{
    RECT wrect;
    GetWindowRect(hWnd, &wrect);
    RECT ret;
    ret.left = wrect.left;
    ret.right = wrect.right;
    ret.top = wrect.top;
    ret.bottom = wrect.top + 32;

    return ret;
}

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
    int current = -1;
    bool isDown = false;

    void OnMousePressed(sf::Mouse::Button button, unsigned int x, unsigned int y, std::vector<sf::RectangleShape>& shapes, ImVec2 impos, ImVec2 imsize, bool snapToGrid, int gridSize)
    {
        if (x > impos.x && x < impos.x + imsize.x &&
            y > impos.y && y < impos.y + imsize.y)
            return;

        if (button == sf::Mouse::Button::Left)
        {
            sf::RectangleShape newRect;
            newRect.setFillColor(sf::Color(180, 180, 180, 156));
            newRect.setOutlineThickness(1);
            newRect.setOutlineColor(sf::Color::Black);
            shapes.push_back(newRect);

            isDown = true;

            pos.push_back(sf::Vector2i(x, y));
            size.push_back(sf::Vector2i(0, 0));

            if (snapToGrid)
            {
                pos[pos.size() - 1].x = round(pos[pos.size() - 1].x - midPt.x, gridSize) + midPt.x;
                pos[pos.size() - 1].y = round(pos[pos.size() - 1].y - midPt.y, gridSize) + midPt.y;
            }

            current = pos.size() - 1;
        }
    }

    void OnMouseReleased(sf::Mouse::Button button, unsigned int x, unsigned int y, std::vector<sf::RectangleShape>& shapes, bool snapToGrid, int gridSize)
    {
        if (button == sf::Mouse::Button::Left)
        {
            if (!isDown) return;

            isDown = false;

            auto tmp = pos[current];
            pos[current].x = std::min(pos[current].x, (int)x);
            pos[current].y = std::min(pos[current].y, (int)y);

            size[current].x = std::max((int)x - tmp.x, tmp.x - (int)x);
            size[current].y = std::max((int)y - tmp.y, tmp.y - (int)y);

            if (snapToGrid)
            {
                size[current].x = round(size[current].x, gridSize);
                size[current].y = round(size[current].y, gridSize);
            }

            for (auto i = 0; i < pos.size(); ++i)
            {
                shapes[i].setPosition(pos[i].x, pos[i].y);
                shapes[i].setSize({ (float)size[i].x, (float)size[i].y });
            }

            if (size[current].x < 10 || size[current].y < 10)
            {
                pos.erase(pos.begin() + current);
                size.erase(size.begin() + current);
                shapes.erase(shapes.begin() + current);
            }

            current = -1;
        }
        else if (button == sf::Mouse::Button::Right)
        {
            if (isDown)
                OnMouseReleased(sf::Mouse::Button::Left, x, y, shapes, snapToGrid, gridSize);

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
};

void EditorWindow(MouseManager& mm, sf::RenderWindow& window, HWND hWnd, std::vector<sf::RectangleShape>& shapes)
{
    ImVec2 impos, imsize;
    bool willSnap = false;
    int willSnapIndex = -1;
    bool wasCtrlClicked = false;
    HWND fhWnd = NULL;
    RECT titleBarRect;
    bool wasHidden = false;
    bool isHidden2 = false;
    int gridSize = 1;
    bool gridShow = false;
    bool snapToGrid = false;

    std::vector<std::unique_ptr<sf::RenderWindow>> windows;
    std::vector<std::array<sf::Vertex, 2>> gridLines;

    sf::Clock deltaClock;
    while (window.isOpen())
    {
        // pause
        wasHidden = isHidden2;
        isHidden2 = isHidden;

        if (isClosed)
            window.close();


        if (isHidden)
        {
            // create windows
            if (isHidden2 && !wasHidden)
            {
                for (auto& w : windows)
                    w->close();
                windows.clear();

                for (auto i = 0; i < mm.pos.size(); ++i)
                {
                    windows.emplace_back(new sf::RenderWindow(sf::VideoMode(mm.size[i].x, mm.size[i].y), "", sf::Style::None));
                    windows[i]->setPosition({ mm.pos[i].x, mm.pos[i].y });
                    windows[i]->clear(rectCol);


                    HWND hWnd = windows[i]->getSystemHandle();
                    DWM_BLURBEHIND bb;
                    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
                    bb.fEnable = true;
                    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
                    DwmEnableBlurBehindWindow(hWnd, &bb);

                    windows[i]->display();
                    SetForegroundWindow(hWnd);
                }
            }

            window.setVisible(false);


            bool isCtrlClicked = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl);
            
            if (fhWnd != GetForegroundWindow())
            {
                fhWnd = GetForegroundWindow();
                for (auto& w : windows)
                    SetForegroundWindow(w->getSystemHandle());
            }

            if (isHidden2 && !wasHidden)
                for (auto& w : windows)
                    w->setVisible(false);

            SetForegroundWindow(fhWnd);
            SetActiveWindow(fhWnd);

            // if pressed title bar
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
            {
                if (fhWnd)
                {
                    auto mx = sf::Mouse::getPosition().x;
                    auto my = sf::Mouse::getPosition().y;

                    titleBarRect = GetTitleBarRect(fhWnd);

                    if (mx > titleBarRect.left &&
                        mx < titleBarRect.right &&
                        my > titleBarRect.top &&
                        my < titleBarRect.bottom)
                    {
                        // ctrl toggle
                        if (!isCtrlClicked && wasCtrlClicked)
                            willSnap = !willSnap;

                        // windows
                        if (willSnap)
                        {
                            for (auto& w : windows)
                                w->setVisible(true);

                            // search for snap rect
                            willSnapIndex = -1;
                            for (auto i = 0; i < mm.pos.size(); ++i)
                            {
                                if (mx > mm.pos[i].x && mx < mm.pos[i].x + mm.size[i].x &&
                                    my > mm.pos[i].y && my < mm.pos[i].y + mm.size[i].y)
                                {
                                    willSnapIndex = i;
                                    break;
                                }
                            }

                            // light up snap rect
                            for (auto i = 0; i < windows.size(); ++i)
                            {
                                if (willSnapIndex == i)
                                    windows[i]->clear(sf::Color(150, 150, 150, 156));
                                else
                                    windows[i]->clear(rectCol);

                                windows[i]->display();
                            }
                        }
                        else
                            for (auto& w : windows)
                                w->setVisible(false);
                    }
                }
            }
            else if (willSnap)
                {
                    willSnap = false;
                    if (willSnapIndex >= 0)
                        MoveWindow(fhWnd, mm.pos[willSnapIndex].x, mm.pos[willSnapIndex].y,
                            mm.size[willSnapIndex].x, mm.size[willSnapIndex].y, true);
                    for (auto& w : windows)
                        w->setVisible(false);
                }

            wasCtrlClicked = isCtrlClicked;
            wasHidden = isHidden2;

            window.display();

            continue;
        }
        else
        {
            window.setVisible(true);
            window.requestFocus();
            SetForegroundWindow(hWnd);
        }

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
                mm.OnMousePressed(event.mouseButton.button, event.mouseButton.x, event.mouseButton.y, shapes, impos, imsize, snapToGrid, gridSize);
                break;
            }
            case sf::Event::MouseButtonReleased:
            {
                mm.OnMouseReleased(event.mouseButton.button, event.mouseButton.x, event.mouseButton.y, shapes, snapToGrid, gridSize);
                break;
            }
            }
        }

        //update

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::Begin("Utilities");

        auto g1 = ImGui::SliderInt("Grid Size", &gridSize, 1, 50);
        auto g2 = ImGui::Checkbox("Show Grid", &gridShow);
        ImGui::SameLine();
        ImGui::Checkbox("Snap To Grid", &snapToGrid);

        auto gg = g1 || g2;

        if (gg)
        {
            gridLines.clear();
            if (gridShow)
            {
                for (auto x = 0; x < midPt.x; x += gridSize)
                {
                    gridLines.emplace_back();
                    gridLines[gridLines.size() - 1][0] = sf::Vertex({ (float)midPt.x + x, 0.f });
                    gridLines[gridLines.size() - 1][0].color = sf::Color(120, 120, 120, 128);
                    gridLines[gridLines.size() - 1][1] = sf::Vertex({ (float)midPt.x + x, (float)screenSize.y });
                    gridLines[gridLines.size() - 1][1].color = sf::Color(120, 120, 120, 128);

                    gridLines.emplace_back();
                    gridLines[gridLines.size() - 1][0] = sf::Vertex({ (float)midPt.x - x, 0.f });
                    gridLines[gridLines.size() - 1][0].color = sf::Color(120, 120, 120, 128);
                    gridLines[gridLines.size() - 1][1] = sf::Vertex({ (float)midPt.x - x, (float)screenSize.y });
                    gridLines[gridLines.size() - 1][1].color = sf::Color(120, 120, 120, 128);
                }
                for (auto y = 0; y < midPt.y; y += gridSize)
                {
                    gridLines.emplace_back();
                    gridLines[gridLines.size() - 1][0] = sf::Vertex({ 0.f, (float)midPt.y + y });
                    gridLines[gridLines.size() - 1][0].color = sf::Color(120, 120, 120, 128);
                    gridLines[gridLines.size() - 1][1] = sf::Vertex({ (float)screenSize.x, (float)midPt.y + y });
                    gridLines[gridLines.size() - 1][1].color = sf::Color(120, 120, 120, 128);

                    gridLines.emplace_back();
                    gridLines[gridLines.size() - 1][0] = sf::Vertex({ 0.f, (float)midPt.y - y });
                    gridLines[gridLines.size() - 1][0].color = sf::Color(120, 120, 120, 128);
                    gridLines[gridLines.size() - 1][1] = sf::Vertex({ (float)screenSize.x, (float)midPt.y - y });
                    gridLines[gridLines.size() - 1][1].color = sf::Color(120, 120, 120, 128);
                }
            }
        }

        if (ImGui::Button("Minimize to Tray"))
        {
            isHidden = true;
        }

        if (ImGui::Button("Exit"))
        {
            isClosed = true;
        }

        impos = ImGui::GetWindowPos();
        imsize = ImGui::GetWindowSize();

        ImGui::End();

        if (mm.isDown)
        {
            auto i = mm.pos.size() - 1;
            shapes[i].setPosition(mm.pos[i].x, mm.pos[i].y);
            shapes[i].setSize(
                { 
                    (float)(sf::Mouse::getPosition().x - mm.pos[i].x), 
                    (float)(sf::Mouse::getPosition().y - mm.pos[i].y)
                });
        }


        // draw

        window.clear(sf::Color(50, 50, 50, 128));

        if (gridShow)
            for (auto& l : gridLines)
                window.draw(l.data(), 2, sf::Lines);

        for (auto& s : shapes)
            window.draw(s);

        ImGui::SFML::Render(window);

        window.display();
    }
}

void TrayHandling()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND thWnd;
    MSG msg;
    WNDCLASS wc = { 0 };
    wc.style = NULL;
    wc.hIcon = NULL;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = NULL;
    wc.lpfnWndProc = WndProc;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = APP_NAME;
    wc.hCursor = NULL;

    if (!RegisterClass(&wc)) return;

    thWnd = CreateWindowEx(WS_EX_TOOLWINDOW, APP_NAME, APP_NAME, WS_POPUP, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    ShowWindow(thWnd, SW_NORMAL);
    UpdateWindow(thWnd);

    InitTray(hInstance, thWnd); // instantiate the tray

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
    window.setFramerateLimit(60);

    ImGui::SFML::Init(window);

    screenSize = sf::Vector2i(sf::VideoMode::getDesktopMode().width, sf::VideoMode::getDesktopMode().height);
    midPt = sf::Vector2{ screenSize.x / 2, screenSize.y / 2 };

    HWND hWnd = window.getSystemHandle();

    DWM_BLURBEHIND bb;
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = true;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hWnd, &bb);

    std::vector<sf::RectangleShape> shapes;

    // tray handling window
    std::thread t(TrayHandling);

    EditorWindow(mm, window, hWnd, shapes);

    ImGui::SFML::Shutdown();

    t.join();

    return 0;
}