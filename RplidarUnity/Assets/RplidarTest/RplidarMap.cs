﻿using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;
using System.Threading;
using System.Linq;

[RequireComponent(typeof(MeshFilter))]
public class RplidarMap : MonoBehaviour
{
    public bool filter = false;
    [Tooltip("(3,10);(0,30) For Trampo space usual lidar spot")]
    public Vector2 filteringDistance,filteringAngle;

    public GameObject _prefab;
    public bool m_onscan = false;

    private LidarData[] m_data;
    public Settings _settings;

    public Mesh m_mesh;
    private List<Vector3> m_vert;
    private List<int> m_ind;

    private MeshFilter m_meshfilter;

    private Thread m_thread;
    private bool m_datachanged = false;
    #region string
    private string data= "";
    #endregion
    void Start()
    {
        RplidarBinding.ReleaseDrive();
        m_meshfilter = GetComponent<MeshFilter>();

        m_data = new LidarData[720];

        m_ind = new List<int>();
        m_vert = new List<Vector3>();
        for (int i = 0; i < 720; i++)
        {
            m_ind.Add(i);
            var cross = Instantiate(_prefab, Vector3.zero, Quaternion.identity, transform);
            _cross.Add(cross);
        }
        m_mesh = new Mesh();
        m_mesh.MarkDynamic();

        RplidarBinding.OnConnect(_settings);
        RplidarBinding.StartMotor();
        m_onscan = RplidarBinding.StartScan();


        if (m_onscan)
        {
            m_thread = new Thread(GenMesh);
            m_thread.Start();
        }

    }
    public bool CheckCondition(Vector3 pos, float theta)
    {
        if (!filter)
            return true;
        return pos.magnitude > filteringDistance.x && pos.magnitude < filteringDistance.y 
            && theta>filteringAngle.x && theta<filteringAngle.y;
    }

    void OnDestroy()
    {
        m_thread.Abort();

        RplidarTest.CleanLidarExit(true);

        m_onscan = false;
    }
    private List<GameObject> _cross = new List<GameObject>();
    private List<float> updateDelays = new List<float>();
    private float _lastUpdate;
    void Update()
    {
        if (m_datachanged)
        {
            float delay = Time.time - _lastUpdate;
            //We start avearging after 3 second
            if(Time.time>3f)
                updateDelays.Add(delay);
            _lastUpdate = Time.time;
            int NbOfPointsFiltered = 0;
            m_vert.Clear();
            for (int i = 0; i < 720; i++)
            {
                float theta = m_data[i].theta;
                Vector3 Pos = Quaternion.Euler(0, 0, (float)theta) * Vector3.right * m_data[i].distant * 0.01f;
                m_vert.Add(Pos * transform.localScale.x);
                if (CheckCondition(Pos,theta))
                {
                    NbOfPointsFiltered++;
                    _cross[i].SetActive(true);
                }
                else
                    _cross[i].SetActive(false);
                _cross[i].transform.position = Pos * transform.localScale.x;
            }
            Debug.Log((delay) + " last update, average is : " + (updateDelays.Sum() / updateDelays.Count) + ", NbOfPoints filered : " + NbOfPointsFiltered);
            m_mesh.SetVertices(m_vert);
            m_mesh.SetIndices(m_ind.ToArray(), MeshTopology.Points, 0);

            m_mesh.UploadMeshData(false);
            m_meshfilter.mesh = m_mesh;

            m_datachanged = false;
        }
    }

    void GenMesh()
    {
        while (true)
        {
            int datacount = RplidarBinding.GetData(ref m_data);
            if (datacount == 0)
            {
                Thread.Sleep(20);
            }
            else
            {
                m_datachanged = true;
            }
        }
    }

}
