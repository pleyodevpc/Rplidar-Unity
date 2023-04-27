using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;


public class RplidarTest : MonoBehaviour
{
    public Settings _settings;
    private string port => _settings.port;
    private LidarData[] data;

    private void Awake()
    {
        data = new LidarData[720];
    }

    // Use this for initialization
    void Start()
    {
    }

    // Update is called once per frame
    void Update()
    {

    }

    private void OnGUI()
    {
        DrawButton("Connect", () =>
        {
            if (string.IsNullOrEmpty(port))
            {
                return;
            }

            int result //= RplidarBinding.OnConnect(port);
            = RplidarBinding.OnConnect(_settings);

            Debug.Log("Connect on " + port + " result:" + result);
        });

        DrawButton("DisConnect", () =>
        {
            bool r = RplidarBinding.OnDisconnect();
            Debug.Log("Disconnect:" + r);
        });

        DrawButton("StartScan", () =>
        {
            bool r = RplidarBinding.StartScan();
            Debug.Log("StartScan:" + r);
        });

        DrawButton("EndScan", () =>
        {
            bool r = RplidarBinding.EndScan();
            Debug.Log("EndScan:" + r);
        });

        DrawButton("StartMotor", () =>
        {
            bool r = RplidarBinding.StartMotor();
            Debug.Log("StartMotor:" + r);
        });

        DrawButton("EndMotor", () =>
        {
            bool r = RplidarBinding.EndMotor();
            Debug.Log("EndMotor:" + r);
        });


        DrawButton("Release Driver", () =>
        {
            bool r = RplidarBinding.ReleaseDrive();
            Debug.Log("Release Driver:" + r);
        });


        DrawButton("GrabData", () =>
        {
            int count = RplidarBinding.GetData(ref data);

            Debug.Log("GrabData:" + count);
            if (count > 0)
            {
                for (int i = 0; i < 20; i++)
                {
                    Debug.Log("d:" + data[i].distant + " " + data[i].quality + " " + data[i].syncBit + " " + data[i].theta);
                }
            }

        });
    }
    private void OnDestroy()
    {
        CleanLidarExit(true);
    }
    public static void CleanLidarExit(bool log = false)
    {
        bool args = RplidarBinding.EndScan();
        bool mot = RplidarBinding.EndMotor();
        bool disc = RplidarBinding.OnDisconnect();
        bool drive = RplidarBinding.ReleaseDrive();
        if (log)
            string.Format("Scan ; {0}, Motor : {1}, Disconnecte {2}, ReleaseDrive{3}", args, mot, disc, drive);
    }
void DrawButton(string label, Action callback)
{
    if (GUILayout.Button(label, GUILayout.Width(200), GUILayout.Height(75)))
    {
        if (callback != null)
        {
            callback.Invoke();
        }
    }
}
}
